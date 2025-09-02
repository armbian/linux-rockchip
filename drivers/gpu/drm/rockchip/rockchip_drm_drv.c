// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * based on exynos_drm_drv.c
 */

#include <linux/dma-buf-cache.h>
#include <linux/dma-mapping.h>
#include <linux/genalloc.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/console.h>
#include <linux/iommu.h>
#include <linux/kthread.h>
#include <linux/of_reserved_mem.h>
#include <uapi/linux/sched/types.h>

#include <drm/drm_aperture.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_drv.h>
#include <drm/drm_displayid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#if defined(CONFIG_ARM_DMA_USE_IOMMU)
#include <asm/dma-iommu.h>
#else
#define arm_iommu_detach_device(...)	({ })
#define arm_iommu_release_mapping(...)	({ })
#define to_dma_iommu_mapping(dev) NULL
#endif

#include "rockchip_drm_drv.h"
#include "rockchip_drm_fb.h"
#include "rockchip_drm_fbdev.h"
#include "rockchip_drm_gem.h"
#include "rockchip_drm_logo.h"

#include "../drm_crtc_internal.h"

#define CREATE_TRACE_POINTS
#include "rockchip_drm_trace.h"

#define DRIVER_NAME	"rockchip"
#define DRIVER_DESC	"RockChip Soc DRM"
#define DRIVER_DATE	"20140818"
#define DRIVER_MAJOR	4
#define DRIVER_MINOR	0

#define for_each_displayid_db(displayid, block, idx, length) \
	for ((block) = (struct displayid_block *)&(displayid)[idx]; \
	    (idx) + sizeof(struct displayid_block) <= (length) && \
	    (idx) + sizeof(struct displayid_block) + (block)->num_bytes <= (length) && \
	    (block)->num_bytes > 0; \
	    (idx) += sizeof(struct displayid_block) + (block)->num_bytes, \
	    (block) = (struct displayid_block *)&(displayid)[idx])

#if IS_ENABLED(CONFIG_DRM_ROCKCHIP_VKMS)
static bool is_support_iommu = false;
#else
static bool is_support_iommu = true;
#endif
static bool iommu_reserve_map;

static const struct drm_driver rockchip_drm_driver;

static unsigned int drm_debug;
module_param_named(debug, drm_debug, int, 0600);

static const u16 tfr_vrefresh_table[TFR_MAX] = {
	[TFR_QMSVRR_INACTIVE] = 0,
	[TFR_23P97] = 2397,
	[TFR_24] = 2400,
	[TFR_25] = 2500,
	[TFR_29P97] = 2997,
	[TFR_30] = 3000,
	[TFR_47P95] = 4795,
	[TFR_48] = 4800,
	[TFR_50] = 5000,
	[TFR_59P94] = 5994,
	[TFR_60] = 6000,
	[TFR_100] = 10000,
	[TFR_119P88] = 11988,
	[TFR_120] = 12000,
};

/* BRR 720p60hz */
static const struct mvrr_const_val const_hdmi720p60_6000 = {
	.vrefresh_khz = 6000,
	.vtotal_fixed = 750,
};

/*
 * @vrefresh_khz:	qms-vrr target refresh rate is 59.94Hz
 * @vtotal_fixed:	When switch to target refresh rate, vtotal is 750
 * @bit_len:		frac_array's bit length
 * @frac_array:		Sources may also alternate between two sequential values
 *			of actual vtotal to better approximate the target refresh
 *			rate when target vtotal is fractional. For this example,
 *			the source vtotal would vary between 750 and 751.
 *			The value in frac_array indicates the order in which the
 *			vtotal changes during this process. Each bit of 0 indicates
 *			that the current frame vtotal is 750, and each bit of 1
 *			indicates that the current frame vtotal is 751.
 *			Take 0x3f(00111111B) as an example, it represents a vtotal
 *			of 750 for the first and second frames, and 751 for the
 *			remaining six frames. 0xe3 and 0xfe also have the same meaning.
 *			The current frac_array represents the value of vtotal for 24
 *			consecutive frames.
 */
static const struct mvrr_const_val const_hdmi720p60_5994 = {
	.vrefresh_khz = 5994,
	.vtotal_fixed = 750, /* 0.75 */
	.bit_len = 24,
	.frac_array = {0x3f, 0xe3, 0xfe},
};

static const struct mvrr_const_val const_hdmi720p60_5000 = {
	.vrefresh_khz = 5000,
	.vtotal_fixed = 900,
};

static const struct mvrr_const_val const_hdmi720p60_4800 = {
	.vrefresh_khz = 4800,
	.vtotal_fixed = 937, /* 0.5 */
	.bit_len = 8,
	.frac_array = {0x3c},
};

static const struct mvrr_const_val const_hdmi720p60_4795 = {
	.vrefresh_khz = 4795,
	.vtotal_fixed = 938, /* 0.4375 */
	.bit_len = 16,
	.frac_array = {0x1e, 0x0e},
};

static const struct mvrr_const_val const_hdmi720p60_3000 = {
	.vrefresh_khz = 3000,
	.vtotal_fixed = 1500,
};

static const struct mvrr_const_val const_hdmi720p60_2997 = {
	.vrefresh_khz = 2997,
	.vtotal_fixed = 1501, /* 0.5 */
	.bit_len = 56,
	.frac_array = {0x1f, 0xe0, 0x3f, 0x80, 0xfe, 0x03, 0xf8},
};

static const struct mvrr_const_val const_hdmi720p60_2500 = {
	.vrefresh_khz = 2500,
	.vtotal_fixed = 1800,
};

static const struct mvrr_const_val const_hdmi720p60_2400 = {
	.vrefresh_khz = 2400,
	.vtotal_fixed = 1875,
};

static const struct mvrr_const_val const_hdmi720p60_2397 = {
	.vrefresh_khz = 2397,
	.vtotal_fixed = 1876, /* 0.875 */
	.bit_len = 40,
	.frac_array = {0x1f, 0xff, 0xff, 0xff, 0xfc},
};

/* BRR 720p120hz */
static const struct mvrr_const_val const_hdmi720p120_12000 = {
	.vrefresh_khz = 12000,
	.vtotal_fixed = 750,
};

static const struct mvrr_const_val const_hdmi720p120_11988 = {
	.vrefresh_khz = 11988,
	.vtotal_fixed = 750, /* 0.75 */
	.bit_len = 24,
	.frac_array = {0x3f, 0xe3, 0xfe},
};

static const struct mvrr_const_val const_hdmi720p120_10000 = {
	.vrefresh_khz = 10000,
	.vtotal_fixed = 900,
};

static const struct mvrr_const_val const_hdmi720p120_6000 = {
	.vrefresh_khz = 6000,
	.vtotal_fixed = 1500,
};

static const struct mvrr_const_val const_hdmi720p120_5994 = {
	.vrefresh_khz = 5994,
	.vtotal_fixed = 1501, /* 0.5 */
	.bit_len = 56,
	.frac_array = {0x0f, 0xe0, 0x3f, 0x80, 0xfe, 0x03, 0xf8},
};

static const struct mvrr_const_val const_hdmi720p120_5000 = {
	.vrefresh_khz = 5000,
	.vtotal_fixed = 1800,
};

static const struct mvrr_const_val const_hdmi720p120_4800 = {
	.vrefresh_khz = 4800,
	.vtotal_fixed = 1875,
};

static const struct mvrr_const_val const_hdmi720p120_4795 = {
	.vrefresh_khz = 4795,
	.vtotal_fixed = 1876, /* 0.875 */
	.bit_len = 40,
	.frac_array = {0x1f, 0xff, 0xff, 0xff, 0xfc},
};

static const struct mvrr_const_val const_hdmi720p120_3000 = {
	.vrefresh_khz = 3000,
	.vtotal_fixed = 3000,
};

static const struct mvrr_const_val const_hdmi720p120_2997 = {
	.vrefresh_khz = 2997,
	.vtotal_fixed = 3003,
};

static const struct mvrr_const_val const_hdmi720p120_2500 = {
	.vrefresh_khz = 2500,
	.vtotal_fixed = 3600,
};

static const struct mvrr_const_val const_hdmi720p120_2400 = {
	.vrefresh_khz = 2400,
	.vtotal_fixed = 3750,
};

static const struct mvrr_const_val const_hdmi720p120_2397 = {
	.vrefresh_khz = 2397,
	.vtotal_fixed = 3753, /* 0.75 */
	.bit_len = 88,
	.frac_array = {
		0x03, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xe0
	},
};

/* BRR 1080p60hz */
static const struct mvrr_const_val const_hdmi1080p60_6000 = {
	.vrefresh_khz = 6000,
	.vtotal_fixed = 1125,
};

static const struct mvrr_const_val const_hdmi1080p60_5994 = {
	.vrefresh_khz = 5994,
	.vtotal_fixed = 1126, /* 0.125 */
	.bit_len = 24,
	.frac_array = {0x00, 0x38, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p60_5000 = {
	.vrefresh_khz = 5000,
	.vtotal_fixed = 1350,
};

static const struct mvrr_const_val const_hdmi1080p60_4800 = {
	.vrefresh_khz = 4800,
	.vtotal_fixed = 1406, /* 0.25 */
	.bit_len = 16,
	.frac_array = {0x03, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p60_4795 = {
	.vrefresh_khz = 4795,
	.vtotal_fixed = 1407, /* 0.65625 */
	.bit_len = 32,
	.frac_array = {0x1f, 0xf1, 0x7f, 0xf0},
};

static const struct mvrr_const_val const_hdmi1080p60_3000 = {
	.vrefresh_khz = 3000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi1080p60_2997 = {
	.vrefresh_khz = 2997,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p60_2500 = {
	.vrefresh_khz = 2500,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi1080p60_2400 = {
	.vrefresh_khz = 2400,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p60_2397 = {
	.vrefresh_khz = 2397,
	.vtotal_fixed = 2815, /* 0.3125 */
	.bit_len = 128,
	.frac_array = {
		0x00, 0x7f, 0x80, 0x00, 0x3f, 0xc0, 0x00, 0x0f,
		0xf0, 0x00, 0x07, 0xf8, 0x00, 0x01, 0xfe, 0x00,
	},
};

/* BRR 1080p120hz */
static const struct mvrr_const_val const_hdmi1080p120_12000 = {
	.vrefresh_khz = 12000,
	.vtotal_fixed = 1125,
};

static const struct mvrr_const_val const_hdmi1080p120_11988 = {
	.vrefresh_khz = 11988,
	.vtotal_fixed = 1126, /* 0.125 */
	.bit_len = 24,
	.frac_array = {0x00, 0x38, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_10000 = {
	.vrefresh_khz = 10000,
	.vtotal_fixed = 1350,
};

static const struct mvrr_const_val const_hdmi1080p120_6000 = {
	.vrefresh_khz = 6000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi1080p120_5994 = {
	.vrefresh_khz = 5994,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_5000 = {
	.vrefresh_khz = 5000,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi1080p120_4800 = {
	.vrefresh_khz = 4800,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p120_4795 = {
	.vrefresh_khz = 4795,
	.vtotal_fixed = 2812, /* 0.3125 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x3f, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p120_3000 = {
	.vrefresh_khz = 3000,
	.vtotal_fixed = 4500,
};

static const struct mvrr_const_val const_hdmi1080p120_2997 = {
	.vrefresh_khz = 2997,
	.vtotal_fixed = 4504, /* 0.5 */
	.bit_len = 32,
	.frac_array = {0x00, 0xff, 0xff, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_2500 = {
	.vrefresh_khz = 2500,
	.vtotal_fixed = 5400,
};

static const struct mvrr_const_val const_hdmi1080p120_2400 = {
	.vrefresh_khz = 2400,
	.vtotal_fixed = 5625,
};

static const struct mvrr_const_val const_hdmi1080p120_2397 = {
	.vrefresh_khz = 2397,
	.vtotal_fixed = 5630, /* 0.625 */
	.bit_len = 48,
	.frac_array = {0x00, 0x7f, 0xff, 0xff, 0xfe, 0x00},
};

/* BRR 2160p60hz */
static const struct mvrr_const_val const_hdmi2160p60_6000 = {
	.vrefresh_khz = 6000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi2160p60_5994 = {
	.vrefresh_khz = 5994,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi2160p60_5000 = {
	.vrefresh_khz = 5000,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi2160p60_4800 = {
	.vrefresh_khz = 4800,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p60_4795 = {
	.vrefresh_khz = 4795,
	.vtotal_fixed = 2815, /* 0.3125 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x3f, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p60_3000 = {
	.vrefresh_khz = 3000,
	.vtotal_fixed = 4500,
};

static const struct mvrr_const_val const_hdmi2160p60_2997 = {
	.vrefresh_khz = 2997,
	.vtotal_fixed = 4504, /* 0.5 */
	.bit_len = 32,
	.frac_array = {0x00, 0xff, 0xff, 0x00},
};

static const struct mvrr_const_val const_hdmi2160p60_2500 = {
	.vrefresh_khz = 2500,
	.vtotal_fixed = 5400,
};

static const struct mvrr_const_val const_hdmi2160p60_2400 = {
	.vrefresh_khz = 2400,
	.vtotal_fixed = 5625,
};

static const struct mvrr_const_val const_hdmi2160p60_2397 = {
	.vrefresh_khz = 2397,
	.vtotal_fixed = 5630, /* 0.625 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0xff, 0xff, 0xff, 0x00},
};

const struct mvrr_const_st const_hdmi1080p60_val = {
	.brr_vic = HDMI_16_1920x1080P60_16x9,
	.val = {
		&const_hdmi1080p60_6000,
		&const_hdmi1080p60_5994,
		&const_hdmi1080p60_5000,
		&const_hdmi1080p60_4800,
		&const_hdmi1080p60_4795,
		&const_hdmi1080p60_3000,
		&const_hdmi1080p60_2997,
		&const_hdmi1080p60_2500,
		&const_hdmi1080p60_2400,
		&const_hdmi1080p60_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi1080p120_val = {
	.brr_vic = HDMI_63_1920x1080P120_16x9,
	.val = {
		&const_hdmi1080p120_12000,
		&const_hdmi1080p120_11988,
		&const_hdmi1080p120_10000,
		&const_hdmi1080p120_6000,
		&const_hdmi1080p120_5994,
		&const_hdmi1080p120_5000,
		&const_hdmi1080p120_4800,
		&const_hdmi1080p120_4795,
		&const_hdmi1080p120_3000,
		&const_hdmi1080p120_2997,
		&const_hdmi1080p120_2500,
		&const_hdmi1080p120_2400,
		&const_hdmi1080p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi720p60_val = {
	.brr_vic = HDMI_4_1280x720P60_16x9,
	.val = {
		&const_hdmi720p60_6000,
		&const_hdmi720p60_5994,
		&const_hdmi720p60_5000,
		&const_hdmi720p60_4800,
		&const_hdmi720p60_4795,
		&const_hdmi720p60_3000,
		&const_hdmi720p60_2997,
		&const_hdmi720p60_2500,
		&const_hdmi720p60_2400,
		&const_hdmi720p60_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi720p120_val = {
	.brr_vic = HDMI_47_1280x720P120_16x9,
	.val = {
		&const_hdmi720p120_12000,
		&const_hdmi720p120_11988,
		&const_hdmi720p120_10000,
		&const_hdmi720p120_6000,
		&const_hdmi720p120_5994,
		&const_hdmi720p120_5000,
		&const_hdmi720p120_4800,
		&const_hdmi720p120_4795,
		&const_hdmi720p120_3000,
		&const_hdmi720p120_2997,
		&const_hdmi720p120_2500,
		&const_hdmi720p120_2400,
		&const_hdmi720p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi2160p60_val = {
	.brr_vic = HDMI_97_3840x2160P60_16x9,
	.val = {
		&const_hdmi2160p60_6000,
		&const_hdmi2160p60_5994,
		&const_hdmi2160p60_5000,
		&const_hdmi2160p60_4800,
		&const_hdmi2160p60_4795,
		&const_hdmi2160p60_3000,
		&const_hdmi2160p60_2997,
		&const_hdmi2160p60_2500,
		&const_hdmi2160p60_2400,
		&const_hdmi2160p60_2397,
		NULL,
	},
};

/* The vtotal parameters of 4096x2160p60hz are the same as 3840x2160p60hz */
const struct mvrr_const_st const_hdmismpte60_val = {
	.brr_vic = HDMI_102_4096x2160P60_256x135,
	.val = {
		&const_hdmi2160p60_6000,
		&const_hdmi2160p60_5994,
		&const_hdmi2160p60_5000,
		&const_hdmi2160p60_4800,
		&const_hdmi2160p60_4795,
		&const_hdmi2160p60_3000,
		&const_hdmi2160p60_2997,
		&const_hdmi2160p60_2500,
		&const_hdmi2160p60_2400,
		&const_hdmi2160p60_2397,
		NULL,
	},
};

const struct mvrr_const_st *qms_const[] = {
	&const_hdmi1080p60_val,
	&const_hdmi1080p120_val,
	&const_hdmi2160p60_val,
	&const_hdmi720p60_val,
	&const_hdmi720p120_val,
	&const_hdmismpte60_val,
	NULL,
};

static inline bool rockchip_drm_debug_enabled(enum rockchip_drm_debug_category category)
{
	return unlikely(drm_debug & category);
}

static void rockchip_drm_dbg_print(const struct device *dev, enum rockchip_drm_debug_category category,
				   bool show_thread, struct va_format *vaf)
{
	if (rockchip_drm_debug_enabled(category)) {
		if (dev) {
			if (show_thread)
				dev_printk(KERN_DEBUG, dev, "%s %pV\n", current->comm, vaf);
			else
				dev_printk(KERN_DEBUG, dev, "%pV\n", vaf);
		} else {
			if (show_thread)
				printk(KERN_DEBUG "%s %pV\n", current->comm, vaf);
			else
				printk(KERN_DEBUG "%pV\n", vaf);
		}
	}

	if (category == VOP_DEBUG_VSYNC)
		trace_rockchip_drm_dbg_vsync(vaf);
	else if (category == VOP_DEBUG_IOMMU_MAP)
		trace_rockchip_drm_dbg_iommu(vaf);
	else
		trace_rockchip_drm_dbg_common(vaf);
}

__printf(3, 4)
void rockchip_drm_dbg(const struct device *dev,
		      enum rockchip_drm_debug_category category,
		      const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	rockchip_drm_dbg_print(dev, category, false, &vaf);

	va_end(args);
}

__printf(3, 4)
void rockchip_drm_dbg_thread_info(const struct device *dev,
				  enum rockchip_drm_debug_category category,
				  const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	rockchip_drm_dbg_print(dev, category, true, &vaf);

	va_end(args);
}

bool rockchip_drm_is_afbc(struct drm_plane *plane, u64 modifier)
{
	int i;

	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return false;

	if (!drm_is_afbc(modifier))
		return false;

	for (i = 0 ; i < plane->modifier_count; i++)
		if (plane->modifiers[i] == modifier)
			break;

	return (i < plane->modifier_count) ? true : false;
}
EXPORT_SYMBOL(rockchip_drm_is_afbc);

bool rockchip_drm_is_rfbc(struct drm_plane *plane, u64 modifier)
{
	int i;

	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return false;

	if (!IS_ROCKCHIP_RFBC_MOD(modifier))
		return false;

	for (i = 0 ; i < plane->modifier_count; i++)
		if (plane->modifiers[i] == modifier)
			break;

	return (i < plane->modifier_count) ? true : false;
}
EXPORT_SYMBOL(rockchip_drm_is_rfbc);

const char *rockchip_drm_modifier_to_string(uint64_t modifier)
{
	switch (modifier) {
	case DRM_FORMAT_MOD_ROCKCHIP_TILED(ROCKCHIP_TILED_BLOCK_SIZE_8x8):
		return "_TILE-8x8";
	case DRM_FORMAT_MOD_ROCKCHIP_TILED(ROCKCHIP_TILED_BLOCK_SIZE_4x4_MODE0):
		return "_TILE-4x4-M0";
	case DRM_FORMAT_MOD_ROCKCHIP_TILED(ROCKCHIP_TILED_BLOCK_SIZE_4x4_MODE1):
		return "_TILE-4x4-M1";
	case DRM_FORMAT_MOD_ROCKCHIP_RFBC(ROCKCHIP_RFBC_BLOCK_SIZE_64x4):
		return "_RFBC-64x4";
	default:
		if (modifier & AFBC_FORMAT_MOD_BLOCK_SIZE_32x8)
			return "_AFBC-32x8";
		else if (modifier & AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)
			return "_AFBC-16x16";
		else
			return "";
	}
}
EXPORT_SYMBOL(rockchip_drm_modifier_to_string);

/**
 * rockchip_drm_wait_vact_end
 * @crtc: CRTC to enable line flag
 * @mstimeout: millisecond for timeout
 *
 * Wait for vact_end line flag irq or timeout.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int rockchip_drm_wait_vact_end(struct drm_crtc *crtc, unsigned int mstimeout)
{
	struct rockchip_drm_private *priv;
	int pipe, ret = 0;

	if (!crtc)
		return -ENODEV;

	if (mstimeout <= 0)
		return -EINVAL;

	priv = crtc->dev->dev_private;
	pipe = drm_crtc_index(crtc);

	if (priv->crtc_funcs[pipe] && priv->crtc_funcs[pipe]->wait_vact_end)
		ret = priv->crtc_funcs[pipe]->wait_vact_end(crtc, mstimeout);

	return ret;
}
EXPORT_SYMBOL(rockchip_drm_wait_vact_end);

void drm_mode_convert_to_split_mode(struct drm_display_mode *mode)
{
	u16 hactive, hfp, hsync, hbp;

	hactive = mode->hdisplay;
	hfp = mode->hsync_start - mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;

	mode->clock *= 2;
	mode->hdisplay = hactive * 2;
	mode->hsync_start = mode->hdisplay + hfp * 2;
	mode->hsync_end = mode->hsync_start + hsync * 2;
	mode->htotal = mode->hsync_end + hbp * 2;

	hactive = mode->crtc_hdisplay;
	hfp = mode->crtc_hsync_start - mode->crtc_hdisplay;
	hsync = mode->crtc_hsync_end - mode->crtc_hsync_start;
	hbp = mode->crtc_htotal - mode->crtc_hsync_end;

	mode->crtc_clock *= 2;
	mode->crtc_hdisplay = hactive * 2;
	mode->crtc_hsync_start = mode->crtc_hdisplay + hfp * 2;
	mode->crtc_hsync_end = mode->crtc_hsync_start + hsync * 2;
	mode->crtc_htotal = mode->crtc_hsync_end + hbp * 2;

	drm_mode_set_name(mode);
}
EXPORT_SYMBOL(drm_mode_convert_to_split_mode);

void drm_mode_convert_to_origin_mode(struct drm_display_mode *mode)
{
	u16 hactive, hfp, hsync, hbp;

	hactive = mode->hdisplay;
	hfp = mode->hsync_start - mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;

	mode->clock /= 2;
	mode->hdisplay = hactive / 2;
	mode->hsync_start = mode->hdisplay + hfp / 2;
	mode->hsync_end = mode->hsync_start + hsync / 2;
	mode->htotal = mode->hsync_end + hbp / 2;

	hactive = mode->crtc_hdisplay;
	hfp = mode->crtc_hsync_start - mode->crtc_hdisplay;
	hsync = mode->crtc_hsync_end - mode->crtc_hsync_start;
	hbp = mode->crtc_htotal - mode->crtc_hsync_end;

	mode->crtc_clock /= 2;
	mode->crtc_hdisplay = hactive / 2;
	mode->crtc_hsync_start = mode->crtc_hdisplay + hfp / 2;
	mode->crtc_hsync_end = mode->crtc_hsync_start + hsync / 2;
	mode->crtc_htotal = mode->crtc_hsync_end + hbp / 2;
}
EXPORT_SYMBOL(drm_mode_convert_to_origin_mode);

uint32_t rockchip_drm_get_bpp(const struct drm_format_info *info)
{
	switch (info->format) {
	case DRM_FORMAT_YUV420_8BIT:
		return 12;
	case DRM_FORMAT_YUV420_10BIT:
		return 15;
	case DRM_FORMAT_VUY101010:
		return 30;
	default:
		return drm_format_info_bpp(info, 0);
	}
}
EXPORT_SYMBOL(rockchip_drm_get_bpp);

uint32_t rockchip_drm_get_cycles_per_pixel(uint32_t bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		return 1;
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
	case MEDIA_BUS_FMT_BGR565_2X8_LE:
		return 2;
	case MEDIA_BUS_FMT_RGB666_3X6:
	case MEDIA_BUS_FMT_RGB888_3X8:
	case MEDIA_BUS_FMT_BGR888_3X8:
		return 3;
	case MEDIA_BUS_FMT_RGB888_DUMMY_4X8:
	case MEDIA_BUS_FMT_BGR888_DUMMY_4X8:
		return 4;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(rockchip_drm_get_cycles_per_pixel);

/**
 * rockchip_drm_of_find_possible_crtcs - find the possible CRTCs for an active
 * encoder port
 * @dev: DRM device
 * @port: encoder port to scan for endpoints
 *
 * Scan all active endpoints attached to a port, locate their attached CRTCs,
 * and generate the DRM mask of CRTCs which may be attached to this
 * encoder.
 *
 * See Documentation/devicetree/bindings/graph.txt for the bindings.
 */
uint32_t rockchip_drm_of_find_possible_crtcs(struct drm_device *dev,
					     struct device_node *port)
{
	struct device_node *remote_port, *ep;
	uint32_t possible_crtcs = 0;

	for_each_endpoint_of_node(port, ep) {
		if (!of_device_is_available(ep))
			continue;

		remote_port = of_graph_get_remote_port(ep);
		if (!remote_port) {
			of_node_put(ep);
			continue;
		}

		possible_crtcs |= drm_of_crtc_port_mask(dev, remote_port);

		of_node_put(remote_port);
	}

	return possible_crtcs;
}
EXPORT_SYMBOL(rockchip_drm_of_find_possible_crtcs);

static DEFINE_MUTEX(rockchip_drm_sub_dev_lock);
static LIST_HEAD(rockchip_drm_sub_dev_list);

void rockchip_connector_update_vfp_for_vrr(struct drm_crtc *crtc, struct drm_display_mode *mode,
					   int vfp)
{
	struct rockchip_drm_sub_dev *sub_dev;

	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_for_each_entry(sub_dev, &rockchip_drm_sub_dev_list, list) {
		if (sub_dev->connector && sub_dev->connector->state->crtc == crtc) {
			if (sub_dev->update_vfp_for_vrr)
				sub_dev->update_vfp_for_vrr(sub_dev->connector, mode, vfp);
		}
	}
	mutex_unlock(&rockchip_drm_sub_dev_lock);
}
EXPORT_SYMBOL(rockchip_connector_update_vfp_for_vrr);

void rockchip_drm_register_sub_dev(struct rockchip_drm_sub_dev *sub_dev)
{
	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_add_tail(&sub_dev->list, &rockchip_drm_sub_dev_list);
	mutex_unlock(&rockchip_drm_sub_dev_lock);
}
EXPORT_SYMBOL(rockchip_drm_register_sub_dev);

void rockchip_drm_unregister_sub_dev(struct rockchip_drm_sub_dev *sub_dev)
{
	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_del(&sub_dev->list);
	mutex_unlock(&rockchip_drm_sub_dev_lock);
}
EXPORT_SYMBOL(rockchip_drm_unregister_sub_dev);

struct rockchip_drm_sub_dev *rockchip_drm_get_sub_dev(struct device_node *node)
{
	struct rockchip_drm_sub_dev *sub_dev = NULL;
	bool found = false;

	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_for_each_entry(sub_dev, &rockchip_drm_sub_dev_list, list) {
		if (sub_dev->of_node == node) {
			found = true;
			break;
		}
	}
	mutex_unlock(&rockchip_drm_sub_dev_lock);

	return found ? sub_dev : NULL;
}
EXPORT_SYMBOL(rockchip_drm_get_sub_dev);

int rockchip_drm_get_sub_dev_type(void)
{
	int connector_type = DRM_MODE_CONNECTOR_Unknown;
	struct rockchip_drm_sub_dev *sub_dev = NULL;

	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_for_each_entry(sub_dev, &rockchip_drm_sub_dev_list, list) {
		if (sub_dev->connector && sub_dev->connector->encoder) {
			connector_type = sub_dev->connector->connector_type;
			break;
		}
	}
	mutex_unlock(&rockchip_drm_sub_dev_lock);

	return connector_type;
}
EXPORT_SYMBOL(rockchip_drm_get_sub_dev_type);

u32 rockchip_drm_get_scan_line_time_ns(void)
{
	struct rockchip_drm_sub_dev *sub_dev = NULL;
	struct drm_display_mode *mode;
	int linedur_ns = 0;

	mutex_lock(&rockchip_drm_sub_dev_lock);
	list_for_each_entry(sub_dev, &rockchip_drm_sub_dev_list, list) {
		if (sub_dev->connector && sub_dev->connector->encoder &&
		    sub_dev->connector->state->crtc) {
			mode = &sub_dev->connector->state->crtc->state->adjusted_mode;
			linedur_ns  = div_u64((u64) mode->crtc_htotal * 1000000, mode->crtc_clock);
			break;
		}
	}
	mutex_unlock(&rockchip_drm_sub_dev_lock);

	return linedur_ns;
}
EXPORT_SYMBOL(rockchip_drm_get_scan_line_time_ns);

void rockchip_drm_te_handle(struct drm_crtc *crtc)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (priv->crtc_funcs[pipe] && priv->crtc_funcs[pipe]->te_handler)
		priv->crtc_funcs[pipe]->te_handler(crtc);
}
EXPORT_SYMBOL(rockchip_drm_te_handle);

struct drm_crtc *
drm_atomic_get_old_crtc_for_encoder(struct drm_atomic_state *state,
				    struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;

	connector = drm_atomic_get_old_connector_for_encoder(state, encoder);
	if (!connector)
		return NULL;

	conn_state = drm_atomic_get_old_connector_state(state, connector);
	if (!conn_state)
		return NULL;

	return conn_state->crtc;
}
EXPORT_SYMBOL(drm_atomic_get_old_crtc_for_encoder);

struct drm_crtc *
drm_atomic_get_new_crtc_for_encoder(struct drm_atomic_state *state,
				    struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;

	connector = drm_atomic_get_new_connector_for_encoder(state, encoder);
	if (!connector)
		return NULL;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state)
		return NULL;

	return conn_state->crtc;
}
EXPORT_SYMBOL(drm_atomic_get_new_crtc_for_encoder);

static const struct drm_display_mode rockchip_drm_default_modes[] = {
	/* 4 - 1280x720@60Hz 16:9 */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 16 - 1920x1080@60Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 31 - 1920x1080@50Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		   2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 19 - 1280x720@50Hz 16:9 */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
		   1760, 1980, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 0x10 - 1024x768@60Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 65000, 1024, 1048,
		   1184, 1344, 0,  768, 771, 777, 806, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 17 - 720x576@50Hz 4:3 */
	{ DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		   796, 864, 0, 576, 581, 586, 625, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* 2 - 720x480@60Hz 4:3 */
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		   798, 858, 0, 480, 489, 495, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
};

int rockchip_drm_add_modes_noedid(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;
	int i, count, num_modes = 0;

	mutex_lock(&rockchip_drm_sub_dev_lock);
	count = ARRAY_SIZE(rockchip_drm_default_modes);

	for (i = 0; i < count; i++) {
		const struct drm_display_mode *ptr = &rockchip_drm_default_modes[i];

		mode = drm_mode_duplicate(dev, ptr);
		if (mode) {
			if (!i)
				mode->type = DRM_MODE_TYPE_PREFERRED;
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}
	mutex_unlock(&rockchip_drm_sub_dev_lock);

	return num_modes;
}
EXPORT_SYMBOL(rockchip_drm_add_modes_noedid);

static const char * const color_encoding_name[] = {
	[DRM_COLOR_YCBCR_BT601] = "BT.601",
	[DRM_COLOR_YCBCR_BT709] = "BT.709",
	[DRM_COLOR_YCBCR_BT2020] = "BT.2020",
};

static const char * const color_range_name[] = {
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = "Limited",
	[DRM_COLOR_YCBCR_FULL_RANGE] = "Full",
};

const char *rockchip_drm_get_color_encoding_name(enum drm_color_encoding encoding)
{
	if (WARN_ON(encoding >= ARRAY_SIZE(color_encoding_name)))
		return "unknown";

	return color_encoding_name[encoding];
}

const char *rockchip_drm_get_color_range_name(enum drm_color_range range)
{
	if (WARN_ON(range >= ARRAY_SIZE(color_range_name)))
		return "unknown";

	return color_range_name[range];
}

static int
cea_db_tag(const u8 *db)
{
	return db[0] >> 5;
}

static int
cea_db_payload_len(const u8 *db)
{
	return db[0] & 0x1f;
}

#define for_each_cea_db(cea, i, start, end) \
	for ((i) = (start); \
	     (i) < (end) && (i) + cea_db_payload_len(&(cea)[(i)]) < (end); \
	     (i) += cea_db_payload_len(&(cea)[(i)]) + 1)

#define HDMI_DOVI_VSDB_OUI 0xd04601

static bool cea_db_is_hdmi_dovi_block(const u8 *db)
{
	unsigned int oui;

	if (cea_db_tag(db) != 0x07)
		return false;

	if (cea_db_payload_len(db) < 11)
		return false;

	oui = db[3] << 16 | db[2] << 8 | db[1];
	return oui == HDMI_DOVI_VSDB_OUI;
}

static bool cea_db_is_hdmi_forum_vsdb(const u8 *db)
{
	unsigned int oui;

	if (cea_db_tag(db) != 0x03)
		return false;

	if (cea_db_payload_len(db) < 7)
		return false;

	oui = db[3] << 16 | db[2] << 8 | db[1];

	return oui == HDMI_FORUM_IEEE_OUI;
}

#define CTA_DB_EXTENDED_TAG		7

static bool cea_db_is_extended_tag(const u8 *db, int tag)
{
	return cea_db_tag(db) == CTA_DB_EXTENDED_TAG &&
		cea_db_payload_len(db) >= 1 &&
		db[1] == tag;
}

#define CTA_EXT_DB_HF_SCDB		0x79

static bool cea_db_is_hdmi_forum_scdb(const u8 *db)
{
	return cea_db_is_extended_tag(db, CTA_EXT_DB_HF_SCDB) &&
		cea_db_payload_len(db) >= 7;
}

static int
cea_db_offsets(const u8 *cea, int *start, int *end)
{
	/* DisplayID CTA extension blocks and top-level CEA EDID
	 * block header definitions differ in the following bytes:
	 *   1) Byte 2 of the header specifies length differently,
	 *   2) Byte 3 is only present in the CEA top level block.
	 *
	 * The different definitions for byte 2 follow.
	 *
	 * DisplayID CTA extension block defines byte 2 as:
	 *   Number of payload bytes
	 *
	 * CEA EDID block defines byte 2 as:
	 *   Byte number (decimal) within this block where the 18-byte
	 *   DTDs begin. If no non-DTD data is present in this extension
	 *   block, the value should be set to 04h (the byte after next).
	 *   If set to 00h, there are no DTDs present in this block and
	 *   no non-DTD data.
	 */
	if (cea[0] == 0x81) {
		/*
		 * for_each_displayid_db() has already verified
		 * that these stay within expected bounds.
		 */
		*start = 3;
		*end = *start + cea[2];
	} else if (cea[0] == 0x02) {
		/* Data block offset in CEA extension block */
		*start = 4;
		*end = cea[2];
		if (*end == 0)
			*end = 127;
		if (*end < 4 || *end > 127)
			return -ERANGE;
	} else {
		return -EOPNOTSUPP;
	}

	return 0;
}

static u8 *find_edid_extension(const struct edid *edid,
			       int ext_id, int *ext_index)
{
	u8 *edid_ext = NULL;
	int i;

	/* No EDID or EDID extensions */
	if (edid == NULL || edid->extensions == 0)
		return NULL;

	/* Find CEA extension */
	for (i = *ext_index; i < edid->extensions; i++) {
		edid_ext = (u8 *)edid + EDID_LENGTH * (i + 1);
		if (edid_ext[0] == ext_id)
			break;
	}

	if (i >= edid->extensions)
		return NULL;

	*ext_index = i + 1;

	return edid_ext;
}

static int validate_displayid(u8 *displayid, int length, int idx)
{
	int i, dispid_length;
	u8 csum = 0;
	struct displayid_header *base;

	base = (struct displayid_header *)&displayid[idx];

	DRM_DEBUG_KMS("base revision 0x%x, length %d, %d %d\n",
		      base->rev, base->bytes, base->prod_id, base->ext_count);

	/* +1 for DispID checksum */
	dispid_length = sizeof(*base) + base->bytes + 1;
	if (dispid_length > length - idx)
		return -EINVAL;

	for (i = 0; i < dispid_length; i++)
		csum += displayid[idx + i];
	if (csum) {
		DRM_NOTE("DisplayID checksum invalid, remainder is %d\n", csum);
		return -EINVAL;
	}

	return 0;
}

static u8 *find_displayid_extension(const struct edid *edid,
				    int *length, int *idx,
				    int *ext_index)
{
	u8 *displayid = find_edid_extension(edid, 0x70, ext_index);
	struct displayid_header *base;
	int ret;

	if (!displayid)
		return NULL;

	/* EDID extensions block checksum isn't for us */
	*length = EDID_LENGTH - 1;
	*idx = 1;

	ret = validate_displayid(displayid, *length, *idx);
	if (ret)
		return NULL;

	base = (struct displayid_header *)&displayid[*idx];
	*length = *idx + sizeof(*base) + base->bytes;

	return displayid;
}

static u8 *find_cea_extension(const struct edid *edid)
{
	int length, idx;
	struct displayid_block *block;
	u8 *cea;
	u8 *displayid;
	int ext_index;

	/* Look for a top level CEA extension block */
	/* FIXME: make callers iterate through multiple CEA ext blocks? */
	ext_index = 0;
	cea = find_edid_extension(edid, 0x02, &ext_index);
	if (cea)
		return cea;

	/* CEA blocks can also be found embedded in a DisplayID block */
	ext_index = 0;
	for (;;) {
		displayid = find_displayid_extension(edid, &length, &idx,
						     &ext_index);
		if (!displayid)
			return NULL;

		idx += sizeof(struct displayid_header);
		for_each_displayid_db(displayid, block, idx, length) {
			if (block->tag == 0x81)
				return (u8 *)block;
		}
	}

	return NULL;
}

#define EDID_CEA_YCRCB422	(1 << 4)

int rockchip_drm_get_yuv422_format(struct drm_connector *connector,
				   const struct edid *edid)
{
	struct drm_display_info *info;
	const u8 *edid_ext;

	if (!connector || !edid)
		return -EINVAL;

	info = &connector->display_info;

	edid_ext = find_cea_extension(edid);
	if (!edid_ext)
		return -EINVAL;

	if (edid_ext[3] & EDID_CEA_YCRCB422)
		info->color_formats |= DRM_COLOR_FORMAT_YCBCR422;

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_get_yuv422_format);

static
void get_max_frl_rate(int max_frl_rate, u8 *max_lanes, u8 *max_rate_per_lane)
{
	switch (max_frl_rate) {
	case 0:
		*max_lanes = 0;
		*max_rate_per_lane = 0;
		break;
	case 1:
		*max_lanes = 3;
		*max_rate_per_lane = 3;
		break;
	case 2:
		*max_lanes = 3;
		*max_rate_per_lane = 6;
		break;
	case 3:
		*max_lanes = 4;
		*max_rate_per_lane = 6;
		break;
	case 4:
		*max_lanes = 4;
		*max_rate_per_lane = 8;
		break;
	case 5:
		*max_lanes = 4;
		*max_rate_per_lane = 10;
		break;
	case 6:
	/*
	 * According to CTS HFR1-17, if max frl rate in edid is out
	 * of hdmi spec range, hdmitx should output its max frl rate.
	 */
	default:
		*max_lanes = 4;
		*max_rate_per_lane = 12;
	}
}

#define EDID_DSC_10BPC			(1 << 0)
#define EDID_DSC_12BPC			(1 << 1)
#define EDID_DSC_16BPC			(1 << 2)
#define EDID_DSC_ALL_BPP		(1 << 3)
#define EDID_DSC_NATIVE_420		(1 << 6)
#define EDID_DSC_1P2			(1 << 7)
#define EDID_DSC_MAX_FRL_RATE_MASK	0xf0
#define EDID_DSC_MAX_SLICES		0xf
#define EDID_DSC_TOTAL_CHUNK_KBYTES	0x3f
#define EDID_MAX_FRL_RATE_MASK		0xf0

#define DRM_EDID_FAPA_START		(1 << 0)
#define DRM_EDID_ALLM			(1 << 1)
#define DRM_EDID_FVA			(1 << 2)
#define DRM_EDID_QMS			(1 << 6)
#define DRM_EDID_VRR_MIN_MASK		0x3f
#define DRM_EDID_VRR_MAX_UPPER_MASK	0xc0
#define DRM_EDID_QMS_TFR_MIN		(1 << 4)
#define DRM_EDID_QMS_TFR_MAX		(1 << 5)

/* Sink Capability Data Structure */
static void parse_hdmi_forum_scds(struct rockchip_drm_hdmi21_data *hdmi21_data, const u8 *hf_scds)
{
	if (hf_scds[7]) {
		u8 max_frl_rate;
		u8 dsc_max_frl_rate;
		u8 dsc_max_slices;

		max_frl_rate = (hf_scds[7] & DRM_EDID_MAX_FRL_RATE_MASK) >> 4;
		if (max_frl_rate)
			DRM_DEBUG_KMS("hdmi21 FRL detected. parsing edid....\n");
		get_max_frl_rate(max_frl_rate, &hdmi21_data->max_lanes,
				 &hdmi21_data->max_frl_rate_per_lane);
		hdmi21_data->dsc_cap.v_1p2 = hf_scds[11] & DRM_EDID_DSC_1P2;

		if (hdmi21_data->dsc_cap.v_1p2) {
			hdmi21_data->dsc_cap.native_420 = hf_scds[11] & DRM_EDID_DSC_NATIVE_420;
			hdmi21_data->dsc_cap.all_bpp = hf_scds[11] & DRM_EDID_DSC_ALL_BPP;

			if (hf_scds[11] & DRM_EDID_DSC_16BPC)
				hdmi21_data->dsc_cap.bpc_supported = 16;
			else if (hf_scds[11] & DRM_EDID_DSC_12BPC)
				hdmi21_data->dsc_cap.bpc_supported = 12;
			else if (hf_scds[11] & DRM_EDID_DSC_10BPC)
				hdmi21_data->dsc_cap.bpc_supported = 10;
			else
				/* Supports min 8 BPC if DSC 1.2 is supported*/
				hdmi21_data->dsc_cap.bpc_supported = 8;

			dsc_max_frl_rate = (hf_scds[12] & DRM_EDID_DSC_MAX_FRL_RATE_MASK) >> 4;
			get_max_frl_rate(dsc_max_frl_rate, &hdmi21_data->dsc_cap.max_lanes,
					 &hdmi21_data->dsc_cap.max_frl_rate_per_lane);
			hdmi21_data->dsc_cap.total_chunk_kbytes =
				hf_scds[13] & DRM_EDID_DSC_TOTAL_CHUNK_KBYTES;

			dsc_max_slices = hf_scds[12] & DRM_EDID_DSC_MAX_SLICES;
			switch (dsc_max_slices) {
			case 1:
				hdmi21_data->dsc_cap.max_slices = 1;
				hdmi21_data->dsc_cap.clk_per_slice = 340;
				break;
			case 2:
				hdmi21_data->dsc_cap.max_slices = 2;
				hdmi21_data->dsc_cap.clk_per_slice = 340;
				break;
			case 3:
				hdmi21_data->dsc_cap.max_slices = 4;
				hdmi21_data->dsc_cap.clk_per_slice = 340;
				break;
			case 4:
				hdmi21_data->dsc_cap.max_slices = 8;
				hdmi21_data->dsc_cap.clk_per_slice = 340;
				break;
			case 5:
				hdmi21_data->dsc_cap.max_slices = 8;
				hdmi21_data->dsc_cap.clk_per_slice = 400;
				break;
			case 6:
				hdmi21_data->dsc_cap.max_slices = 12;
				hdmi21_data->dsc_cap.clk_per_slice = 400;
				break;
			case 7:
				hdmi21_data->dsc_cap.max_slices = 16;
				hdmi21_data->dsc_cap.clk_per_slice = 400;
				break;
			case 0:
			default:
				hdmi21_data->dsc_cap.max_slices = 0;
				hdmi21_data->dsc_cap.clk_per_slice = 0;
			}
		}
	}

	/* parse additional bytes */
	if (cea_db_payload_len(hf_scds) >= 9) {
		hdmi21_data->allm_supported = hf_scds[8] & DRM_EDID_ALLM;
		hdmi21_data->vrr_cap.qms = hf_scds[8] & DRM_EDID_QMS;
		hdmi21_data->vrr_cap.fva = hf_scds[8] & DRM_EDID_FVA;
		hdmi21_data->vrr_cap.m_delta = hf_scds[8] & DRM_EDID_MDELTA;
		hdmi21_data->vrr_cap.negm_vrr = hf_scds[8] & DRM_EDID_CNMVRR;
		hdmi21_data->vrr_cap.cinema_vrr = hf_scds[8] & DRM_EDID_CINEMA_VRR;
	}

	if (cea_db_payload_len(hf_scds) >= 11) {
		hdmi21_data->vrr_cap.vrr_min = hf_scds[9] & DRM_EDID_VRR_MIN_MASK;
		hdmi21_data->vrr_cap.vrr_max =
			(hf_scds[9] & DRM_EDID_VRR_MAX_UPPER_MASK) << 2 | hf_scds[10];
	}

	if (cea_db_payload_len(hf_scds) >= 12) {
		hdmi21_data->vrr_cap.qms_tfr_min = hf_scds[11] & DRM_EDID_QMS_TFR_MIN;
		hdmi21_data->vrr_cap.qms_tfr_max = hf_scds[11] & DRM_EDID_QMS_TFR_MAX;
	}
}

static
int parse_dovi_block(u8 *sink_data, const u8 *dovi_db)
{
	u8 length = (dovi_db[0] & 0x1f) + 1;

	if (length > DOVI_VSDB_LEN)
		return -EINVAL;

	memcpy(sink_data, dovi_db, length);
	return 0;
}

int rockchip_drm_parse_cea_ext(struct rockchip_drm_hdmi21_data *hdmi21_data,
			       const struct edid *edid)
{
	const u8 *edid_ext;
	int i, start, end;

	if (!hdmi21_data || !edid)
		return -EINVAL;

	edid_ext = find_cea_extension(edid);
	if (!edid_ext)
		return -EINVAL;

	if (cea_db_offsets(edid_ext, &start, &end))
		return -EINVAL;

	for_each_cea_db(edid_ext, i, start, end) {
		const u8 *db = &edid_ext[i];

		if (cea_db_is_hdmi_forum_vsdb(db) || cea_db_is_hdmi_forum_scdb(db))
			parse_hdmi_forum_scds(hdmi21_data, db);
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_parse_cea_ext);

int rockchip_drm_parse_dovi(u8 *sink_data, const struct edid *edid)
{
	const u8 *edid_ext;
	int i, start, end, ret;

	if (!sink_data || !edid)
		return -EINVAL;

	memset(sink_data, 0, DOVI_VSDB_LEN);

	edid_ext = find_cea_extension(edid);
	if (!edid_ext)
		return -EINVAL;

	if (cea_db_offsets(edid_ext, &start, &end))
		return -EINVAL;

	for_each_cea_db(edid_ext, i, start, end) {
		const u8 *db = &edid_ext[i];

		if (cea_db_is_hdmi_dovi_block(db)) {
			ret = parse_dovi_block(sink_data, db);
			if (ret)
				return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_parse_dovi);

#define COLORIMETRY_DATA_BLOCK		0x5
#define USE_EXTENDED_TAG		0x07

static bool cea_db_is_hdmi_colorimetry_data_block(const u8 *db)
{
	if (cea_db_tag(db) != USE_EXTENDED_TAG)
		return false;

	if (db[1] != COLORIMETRY_DATA_BLOCK)
		return false;

	return true;
}

int
rockchip_drm_parse_colorimetry_data_block(u8 *colorimetry, const struct edid *edid)
{
	const u8 *edid_ext;
	int i, start, end;

	if (!colorimetry || !edid)
		return -EINVAL;

	*colorimetry = 0;

	edid_ext = find_cea_extension(edid);
	if (!edid_ext)
		return -EINVAL;

	if (cea_db_offsets(edid_ext, &start, &end))
		return -EINVAL;

	for_each_cea_db(edid_ext, i, start, end) {
		const u8 *db = &edid_ext[i];

		if (cea_db_is_hdmi_colorimetry_data_block(db))
			/* As per CEA 861-G spec */
			*colorimetry = ((db[3] & (0x1 << 7)) << 1) | db[2];
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_parse_colorimetry_data_block);

#define HDR10_PLUS_OUI 0x90848b

static bool cea_db_is_hdr10_plus_block(const u8 *db)
{
	unsigned int oui;

	if (cea_db_tag(db) != CTA_DB_EXTENDED_TAG)
		return false;

	if (cea_db_payload_len(db) < 5)
		return false;

	oui = db[4] << 16 | db[3] << 8 | db[2];
	return oui == HDR10_PLUS_OUI;
}

u8 rockchip_drm_parse_hdr10_plus_vsdb(const struct edid *edid)
{
	const u8 *edid_ext;
	int i, start, end;
	u8 hdr10_plus = 0;

	if (!edid)
		return 0;

	edid_ext = find_cea_extension(edid);
	if (!edid_ext)
		return 0;

	if (cea_db_offsets(edid_ext, &start, &end))
		return 0;

	for_each_cea_db(edid_ext, i, start, end) {
		const u8 *db = &edid_ext[i];

		if (cea_db_is_hdr10_plus_block(db))
			/* As per CEA 861-G spec */
			hdr10_plus = db[5];
	}

	return hdr10_plus;
}
EXPORT_SYMBOL(rockchip_drm_parse_hdr10_plus_vsdb);

/*
 * Attach a (component) device to the shared drm dma mapping from master drm
 * device.  This is used by the VOPs to map GEM buffers to a common DMA
 * mapping.
 */
int rockchip_drm_dma_attach_device(struct drm_device *drm_dev,
				   struct device *dev)
{
	struct rockchip_drm_private *private = drm_dev->dev_private;
	int ret;

	if (!private->domain)
		return 0;

	if (IS_ENABLED(CONFIG_ARM_DMA_USE_IOMMU)) {
		struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);

		if (mapping) {
			arm_iommu_detach_device(dev);
			arm_iommu_release_mapping(mapping);
		}
	}

	ret = iommu_attach_device(private->domain, dev);
	if (ret) {
		DRM_DEV_ERROR(dev, "Failed to attach iommu device\n");
		return ret;
	}

	return 0;
}

void rockchip_drm_dma_detach_device(struct drm_device *drm_dev,
				    struct device *dev)
{
	struct rockchip_drm_private *private = drm_dev->dev_private;

	if (!private->domain)
		return;

	iommu_detach_device(private->domain, dev);
}

void rockchip_drm_dma_init_device(struct drm_device *drm_dev,
				  struct device *dev)
{
	struct rockchip_drm_private *private = drm_dev->dev_private;

	if (!device_iommu_mapped(dev))
		private->iommu_dev = ERR_PTR(-ENODEV);
	else if (!private->iommu_dev)
		private->iommu_dev = dev;
}

void rockchip_drm_crtc_standby(struct drm_crtc *crtc, bool standby)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (pipe < ROCKCHIP_MAX_CRTC &&
	    priv->crtc_funcs[pipe] &&
	    priv->crtc_funcs[pipe]->crtc_standby)
		priv->crtc_funcs[pipe]->crtc_standby(crtc, standby);
}

void rockchip_drm_crtc_output_post_enable(struct drm_crtc *crtc, int intf)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (pipe < ROCKCHIP_MAX_CRTC &&
	    priv->crtc_funcs[pipe] &&
	    priv->crtc_funcs[pipe]->crtc_output_post_enable)
		priv->crtc_funcs[pipe]->crtc_output_post_enable(crtc, intf);
}

void rockchip_drm_crtc_output_pre_disable(struct drm_crtc *crtc, int intf)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (pipe < ROCKCHIP_MAX_CRTC &&
	    priv->crtc_funcs[pipe] &&
	    priv->crtc_funcs[pipe]->crtc_output_pre_disable)
		priv->crtc_funcs[pipe]->crtc_output_pre_disable(crtc, intf);
}

int rockchip_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct rockchip_crtc_funcs *crtc_funcs)
{
	int pipe = drm_crtc_index(crtc);
	struct rockchip_drm_private *priv = crtc->dev->dev_private;

	if (pipe >= ROCKCHIP_MAX_CRTC)
		return -EINVAL;

	priv->crtc_funcs[pipe] = crtc_funcs;

	return 0;
}

void rockchip_unregister_crtc_funcs(struct drm_crtc *crtc)
{
	int pipe = drm_crtc_index(crtc);
	struct rockchip_drm_private *priv = crtc->dev->dev_private;

	if (pipe >= ROCKCHIP_MAX_CRTC)
		return;

	priv->crtc_funcs[pipe] = NULL;
}

u16 rockchip_hdmi_vrr_tfr_match_to_vrefresh(u8 tfr)
{
	if (tfr < 0 || tfr >= TFR_MAX) {
		DRM_ERROR("qms-vrr tfr is out of range\n");
		return 0;
	}

	return tfr_vrefresh_table[tfr];
}

const struct
mvrr_const_val *rockchip_hdmi_vrr_get_vrrconf_mconst(enum hdmi_brr_vic brr_vic, u16 vrefresh_khz)
{
	const struct mvrr_const_st **table_vic = NULL;
	const struct mvrr_const_val *const *table_val = NULL;

	for (table_vic = qms_const; table_vic; table_vic++) {
		if ((*table_vic)->brr_vic == brr_vic) {
			table_val = (*table_vic)->val;
			for (; table_val; table_val++) {
				if ((*table_val)->vrefresh_khz == vrefresh_khz)
					break;
			}
			break;
		}
	}

	if (!table_val) {
		DRM_ERROR("%s[%d] not find brr_vic: %d vrefresh_khz: %d\n",
			  __func__, __LINE__, brr_vic, vrefresh_khz);
		return NULL;
	}

	return *table_val;
}

u16 rockchip_hdmi_vrr_calc_new_vtotal(const struct mvrr_const_val *mvrr, u32 frame_cnt)
{
	u32 pos;
	u16 vtotal = 0;

	if (!mvrr)
		return vtotal;

	vtotal = mvrr->vtotal_fixed;
	/* if bit_len is 0, then means there is no fraction */
	if (!mvrr->bit_len)
		return vtotal;

	/* calculate the fraction number */
	pos = frame_cnt % mvrr->bit_len;
	if (mvrr->frac_array[pos / 8] & (1 << (7 - (pos % 8))))
		vtotal++;

	return vtotal;
}

/*
 * a high frequency of page faults will follow up, if
 * there is a iommu fault, so it's better to limit the
 * registers dump frequency to save log buffer
 *
 * Report no more than once every 10s, give userspace time
 * to do recovery process, as for a serdes based display
 * pipeline, the disable/enable time may very long.
 */
static DEFINE_RATELIMIT_STATE(fault_handler_rate, 10 * HZ, 1);

static int fault_handler_rate_limit(void)
{
	return __ratelimit(&fault_handler_rate);
}

void rockchip_drm_reset_iommu_fault_handler_rate_limit(void)
{
	fault_handler_rate.begin = 0;
	fault_handler_rate.printed = 0;
}

static int rockchip_drm_fault_handler(struct iommu_domain *iommu,
				      struct device *dev,
				      unsigned long iova, int flags, void *arg)
{
	struct drm_device *drm_dev = arg;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc;
	bool handled = false;

	DRM_ERROR("iommu fault handler flags: 0x%x: count: %lld\n",
		  flags, ++priv->iommu_fault_count);

	if (!fault_handler_rate_limit())
		return 0;

	drm_for_each_crtc(crtc, drm_dev) {
		int pipe = drm_crtc_index(crtc);

		/*
		 * Only need to call iommu fault handler once for one iommu fault
		 */
		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->iommu_fault_handler &&
		    !handled) {
			priv->crtc_funcs[pipe]->iommu_fault_handler(crtc, iommu);
			handled = true;
		}

		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->regs_dump)
			priv->crtc_funcs[pipe]->regs_dump(crtc, NULL);

		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->debugfs_dump)
			priv->crtc_funcs[pipe]->debugfs_dump(crtc, NULL);
	}

	return 0;
}

static int rockchip_drm_init_iommu(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *private = drm_dev->dev_private;
	struct iommu_domain_geometry *geometry;
	u64 start, end;
	int ret = 0;

	if (IS_ERR_OR_NULL(private->iommu_dev))
		return 0;

	private->domain = iommu_domain_alloc(private->iommu_dev->bus);
	if (!private->domain)
		return -ENOMEM;

	geometry = &private->domain->geometry;
	start = geometry->aperture_start;
	end = geometry->aperture_end;

	DRM_DEBUG("IOMMU context initialized (aperture: %#llx-%#llx)\n",
		  start, end);
	drm_mm_init(&private->mm, start, end - start + 1);
	mutex_init(&private->mm_lock);

	iommu_set_fault_handler(private->domain, rockchip_drm_fault_handler,
				drm_dev);

	if (iommu_reserve_map) {
		/*
		 * At 32 bit platform size_t maximum value is 0xffffffff, SZ_4G(0x100000000) will be
		 * cliped to 0, so we split into two mapping
		 */
		ret = iommu_map(private->domain, 0, 0, (size_t)SZ_2G,
				IOMMU_WRITE | IOMMU_READ | IOMMU_PRIV);
		if (ret) {
			dev_err(drm_dev->dev, "failed to create 0-2G pre mapping\n");
			return 0;
		}

		ret = iommu_map(private->domain, SZ_2G, SZ_2G, (size_t)SZ_2G,
				IOMMU_WRITE | IOMMU_READ | IOMMU_PRIV);
		if (ret) {
			dev_err(drm_dev->dev, "failed to create 2G-4G pre mapping\n");
			return 0;
		}
		dev_info(drm_dev->dev, "Enable iommu reserve map\n");
	}

	return ret;
}

static void rockchip_iommu_cleanup(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *private = drm_dev->dev_private;

	if (!private->domain)
		return;

	if (iommu_reserve_map) {
		iommu_unmap(private->domain, 0, (size_t)SZ_2G);
		iommu_unmap(private->domain, SZ_2G, (size_t)SZ_2G);
	}
	drm_mm_takedown(&private->mm);
	iommu_domain_free(private->domain);
}

#ifdef CONFIG_DEBUG_FS
static int rockchip_drm_mm_dump(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct drm_minor *minor = node->minor;
	struct drm_device *drm_dev = minor->dev;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_printer p = drm_seq_file_printer(s);

	if (!priv->domain)
		return 0;
	mutex_lock(&priv->mm_lock);
	drm_mm_print(&priv->mm, &p);
	mutex_unlock(&priv->mm_lock);

	return 0;
}

static int rockchip_drm_summary_show(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct drm_minor *minor = node->minor;
	struct drm_device *drm_dev = minor->dev;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev) {
		int pipe = drm_crtc_index(crtc);

		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->debugfs_dump)
			priv->crtc_funcs[pipe]->debugfs_dump(crtc, s);
	}

	return 0;
}

static int rockchip_drm_regs_dump(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct drm_minor *minor = node->minor;
	struct drm_device *drm_dev = minor->dev;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev) {
		int pipe = drm_crtc_index(crtc);

		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->regs_dump)
			priv->crtc_funcs[pipe]->regs_dump(crtc, s);
	}

	return 0;
}

static int rockchip_drm_active_regs_dump(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct drm_minor *minor = node->minor;
	struct drm_device *drm_dev = minor->dev;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev) {
		int pipe = drm_crtc_index(crtc);

		if (priv->crtc_funcs[pipe] &&
		    priv->crtc_funcs[pipe]->active_regs_dump)
			priv->crtc_funcs[pipe]->active_regs_dump(crtc, s);
	}

	return 0;
}

static struct drm_info_list rockchip_debugfs_files[] = {
	{ "active_regs", rockchip_drm_active_regs_dump, 0, NULL },
	{ "regs", rockchip_drm_regs_dump, 0, NULL },
	{ "summary", rockchip_drm_summary_show, 0, NULL },
	{ "mm_dump", rockchip_drm_mm_dump, 0, NULL },
};

static void rockchip_drm_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_create_files(rockchip_debugfs_files,
				 ARRAY_SIZE(rockchip_debugfs_files),
				 minor->debugfs_root, minor);
}
#endif

static const struct drm_prop_enum_list split_area[] = {
	{ ROCKCHIP_DRM_SPLIT_UNSET, "UNSET" },
	{ ROCKCHIP_DRM_SPLIT_LEFT_SIDE, "LEFT" },
	{ ROCKCHIP_DRM_SPLIT_RIGHT_SIDE, "RIGHT" },
};

static int rockchip_drm_create_properties(struct drm_device *dev)
{
	struct drm_property *prop;
	struct rockchip_drm_private *private = dev->dev_private;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "EOTF", 0, HDMI_EOTF_DOVI);
	if (!prop)
		return -ENOMEM;
	private->eotf_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "ASYNC_COMMIT", 0, 1);
	if (!prop)
		return -ENOMEM;
	private->async_commit_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "SHARE_ID", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	private->share_id_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_IMMUTABLE,
					 "CONNECTOR_ID", 0, 0xf);
	if (!prop)
		return -ENOMEM;
	private->connector_id_prop = prop;

	prop = drm_property_create_enum(dev, DRM_MODE_PROP_ENUM, "SPLIT_AREA",
					split_area,
					ARRAY_SIZE(split_area));
	private->split_area_prop = prop;

	prop = drm_property_create_object(dev,
					  DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_IMMUTABLE,
					  "SOC_ID", DRM_MODE_OBJECT_CRTC);
	private->soc_id_prop = prop;

	prop = drm_property_create_object(dev,
					  DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_IMMUTABLE,
					  "PORT_ID", DRM_MODE_OBJECT_CRTC);
	private->port_id_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "DOVI_INPUT_TYPE", 0, DOVI_ENHANCE_LAYER);
	if (!prop)
		return -ENOMEM;
	private->dovi_input_type_prop = prop;

	private->aclk_prop = drm_property_create_range(dev, 0, "ACLK", 0, UINT_MAX);
	private->bg_prop = drm_property_create_range(dev, 0, "BACKGROUND", 0, UINT_MAX);
	private->line_flag_prop = drm_property_create_range(dev, 0, "LINE_FLAG1", 0, UINT_MAX);
	private->cubic_lut_prop = drm_property_create(dev, DRM_MODE_PROP_BLOB, "CUBIC_LUT", 0);
	private->cubic_lut_size_prop = drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE,
								 "CUBIC_LUT_SIZE", 0, UINT_MAX);

	private->dimming_data_prop = drm_property_create(dev, DRM_MODE_PROP_BLOB,
							 "DIMMING_DATA", 0);

	return drm_mode_create_tv_properties(dev, 0, NULL);
}

static void rockchip_attach_connector_property(struct drm_device *drm)
{
	struct drm_connector *connector;
	struct drm_mode_config *conf = &drm->mode_config;
	struct drm_connector_list_iter conn_iter;

	mutex_lock(&drm->mode_config.mutex);

#define ROCKCHIP_PROP_ATTACH(prop, v) \
		drm_object_attach_property(&connector->base, prop, v)

	drm_connector_list_iter_begin(drm, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		ROCKCHIP_PROP_ATTACH(conf->tv_brightness_property, 50);
		ROCKCHIP_PROP_ATTACH(conf->tv_contrast_property, 50);
		ROCKCHIP_PROP_ATTACH(conf->tv_saturation_property, 50);
		ROCKCHIP_PROP_ATTACH(conf->tv_hue_property, 50);
	}
	drm_connector_list_iter_end(&conn_iter);
#undef ROCKCHIP_PROP_ATTACH

	mutex_unlock(&drm->mode_config.mutex);
}

static void rockchip_drm_set_property_default(struct drm_device *drm)
{
	struct drm_connector *connector;
	struct drm_mode_config *conf = &drm->mode_config;
	struct drm_atomic_state *state;
	int ret;
	struct drm_connector_list_iter conn_iter;

	drm_modeset_lock_all(drm);

	state = drm_atomic_helper_duplicate_state(drm, conf->acquire_ctx);
	if (IS_ERR(state)) {
		DRM_ERROR("failed to alloc atomic state\n");
		goto err_unlock;
	}
	state->acquire_ctx = conf->acquire_ctx;

	drm_connector_list_iter_begin(drm, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		struct drm_connector_state *connector_state;

		connector_state = drm_atomic_get_connector_state(state,
								 connector);
		if (IS_ERR(connector_state)) {
			DRM_ERROR("Connector[%d]: Failed to get state\n", connector->base.id);
			continue;
		}

		connector_state->tv.brightness = 50;
		connector_state->tv.contrast = 50;
		connector_state->tv.saturation = 50;
		connector_state->tv.hue = 50;
	}
	drm_connector_list_iter_end(&conn_iter);

	ret = drm_atomic_commit(state);
	WARN_ON(ret == -EDEADLK);
	if (ret)
		DRM_ERROR("Failed to update properties\n");
	drm_atomic_state_put(state);

err_unlock:
	drm_modeset_unlock_all(drm);
}

static int rockchip_gem_pool_init(struct drm_device *drm)
{
	struct rockchip_drm_private *private = drm->dev_private;
	struct device_node *np = drm->dev->of_node;
	struct device_node *node;
	phys_addr_t start, size;
	struct resource res;
	int ret;

	node = of_parse_phandle(np, "secure-memory-region", 0);
	if (!node)
		return -ENXIO;

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		return ret;
	start = res.start;
	size = resource_size(&res);
	if (!size)
		return -ENOMEM;

	private->secure_buffer_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!private->secure_buffer_pool)
		return -ENOMEM;

	gen_pool_add(private->secure_buffer_pool, start, size, -1);

	return 0;
}

static void rockchip_gem_pool_destroy(struct drm_device *drm)
{
	struct rockchip_drm_private *private = drm->dev_private;

	if (!private->secure_buffer_pool)
		return;

	gen_pool_destroy(private->secure_buffer_pool);
}

static int rockchip_drm_sysfs_init(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct device *dev;
	struct drm_crtc *crtc = NULL;
	int pipe, ret;

	drm_for_each_crtc(crtc, drm_dev) {
		dev = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!dev)
			return -ENOMEM;

		ret = dev_set_name(dev, "%s", crtc->name);
		if (ret)
			goto cleanup;

		dev->parent = drm_dev->primary->kdev;
		ret = device_register(dev);
		if (ret) {
			put_device(dev);
			goto cleanup;
		}

		dev_set_drvdata(dev, crtc);
		pipe = drm_crtc_index(crtc);
		priv->sysfs_devs[pipe] = dev;

		if (priv->crtc_funcs[pipe] && priv->crtc_funcs[pipe]->sysfs_init)
			priv->crtc_funcs[pipe]->sysfs_init(dev, crtc);
	}

	return 0;

cleanup:
	kfree(dev);
	return ret;
}

static void rockchip_drm_sysfs_fini(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc = NULL;
	struct device *dev;
	int pipe;

	drm_for_each_crtc(crtc, drm_dev) {
		pipe = drm_crtc_index(crtc);
		dev = priv->sysfs_devs[pipe];

		if (dev) {
			if (priv->crtc_funcs[pipe] && priv->crtc_funcs[pipe]->sysfs_fini)
				priv->crtc_funcs[pipe]->sysfs_fini(dev, crtc);
			device_unregister(dev);
			kfree(dev);
			priv->sysfs_devs[pipe] = NULL;
		}
	}
}

void rockchip_drm_send_error_event(struct rockchip_drm_private *priv,
				   enum rockchip_drm_error_event_type event)
{
	struct rockchip_drm_error_event *error_event = &priv->error_event;
	struct drm_event_vblank *e;
	struct timespec64 tv;
	unsigned long flags;

	/*
	 * Maybe the error thread has not be created.
	 */
	if (IS_ERR_OR_NULL(priv->error_event.thread))
		return;

	spin_lock_irqsave(&error_event->lock, flags);
	tv = ktime_to_timespec64(ktime_get());
	e = &error_event->event;
	e->base.type = event;
	e->base.length = sizeof(*e);
	e->tv_sec = tv.tv_sec;
	e->tv_usec = tv.tv_nsec / 1000;
	e->sequence++;
	error_event->error_state = true;
	spin_unlock_irqrestore(&error_event->lock, flags);

	wake_up_interruptible_all(&error_event->wait);
}

static int rockchip_drm_error_event_thread(void *data)
{
	struct drm_device *drm_dev = data;
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct rockchip_drm_error_event *error_event = &priv->error_event;
	struct drm_event_vblank *e;
	int ret = 0;
	int cnt = 0;

	while (!kthread_should_stop()) {
		e = &error_event->event;

		error_event->error_state = false;
		ret = wait_event_interruptible(error_event->wait, error_event->error_state);
		if (!ret) {
			sysfs_notify(&drm_dev->dev->kobj, NULL, "error_event");
			drm_info(drm_dev, "rockchipdrm send_error_event_type: 0x%x, count:%d\n",
				 e->base.type, ++cnt);
		}
	}

	return 0;
}

static ssize_t rockchip_drm_error_event_show(struct device *dev,
					     struct device_attribute *attr, char *buf)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	struct rockchip_drm_error_event *error_event = &priv->error_event;
	struct drm_event_vblank *e;
	uint32_t length = sizeof(*e);
	unsigned long flags;

	spin_lock_irqsave(&error_event->lock, flags);
	e = &error_event->event;
	memcpy(buf, e, length);
	spin_unlock_irqrestore(&error_event->lock, flags);

	return length;
}
static DEVICE_ATTR(error_event, 0444, rockchip_drm_error_event_show, NULL);

static void rockchip_drm_error_event_init(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *priv = drm_dev->dev_private;
	int ret;

	ret = device_create_file(drm_dev->dev, &dev_attr_error_event);
	if (ret) {
		dev_warn(drm_dev->dev, "failed to create vcnt event file\n");
		return;
	}

	init_waitqueue_head(&priv->error_event.wait);
	spin_lock_init(&priv->error_event.lock);
	priv->error_event.thread = kthread_run(rockchip_drm_error_event_thread,
					       drm_dev, "display-error-event-thread");
	if (IS_ERR(priv->error_event.thread)) {
		priv->error_event.thread = NULL;
		drm_err(drm_dev, "failed to run display error_event thread\n");
	} else {
		sched_set_fifo_low(priv->error_event.thread);
		drm_info(drm_dev, "run display error_event monitor\n");
	}
}

static void rockchip_drm_error_event_fini(struct drm_device *drm_dev)
{
	struct rockchip_drm_private *priv = drm_dev->dev_private;

	if (priv->error_event.thread)
		kthread_stop(priv->error_event.thread);
	device_remove_file(drm_dev->dev, &dev_attr_error_event);
}

int rockchip_drm_panel_loader_protect(struct drm_panel *panel, bool on)
{
	struct rockchip_drm_sub_dev *sub_dev;

	if (!panel)
		return -EINVAL;

	sub_dev = rockchip_drm_get_sub_dev(panel->dev->of_node);
	if (sub_dev && sub_dev->loader_protect)
		return sub_dev->loader_protect(sub_dev, on);

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_panel_loader_protect);

static void rockchip_drm_fix_encoder_possible_clones(struct drm_encoder *encoder)
{
	struct drm_device *drm_dev = encoder->dev;
	struct drm_encoder *other;

	drm_for_each_encoder(other, drm_dev) {
		if (other->possible_crtcs & encoder->possible_crtcs)
			encoder->possible_clones |= drm_encoder_mask(other);
	}
}

static int rockchip_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	struct rockchip_drm_private *private;
	struct drm_encoder *encoder;
	int ret;

	/* Remove existing drivers that may own the framebuffer memory. */
	ret = drm_aperture_remove_framebuffers(&rockchip_drm_driver);
	if (ret) {
		DRM_DEV_ERROR(dev,
			      "Failed to remove existing framebuffers - %d.\n",
			      ret);
		return ret;
	}

	drm_dev = drm_dev_alloc(&rockchip_drm_driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);

	dev_set_drvdata(dev, drm_dev);

	private = devm_kzalloc(drm_dev->dev, sizeof(*private), GFP_KERNEL);
	if (!private) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&private->ovl_lock);

	drm_dev->dev_private = private;

	private->hdmi_pll.pll = devm_clk_get_optional(dev, "hdmi-tmds-pll");
	if (PTR_ERR(private->hdmi_pll.pll) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto err_free;
	} else if (IS_ERR(private->hdmi_pll.pll)) {
		dev_err(dev, "failed to get hdmi-tmds-pll\n");
		ret = PTR_ERR(private->hdmi_pll.pll);
		goto err_free;
	}
	private->default_pll.pll = devm_clk_get_optional(dev, "default-vop-pll");
	if (PTR_ERR(private->default_pll.pll) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto err_free;
	} else if (IS_ERR(private->default_pll.pll)) {
		dev_err(dev, "failed to get default vop pll\n");
		ret = PTR_ERR(private->default_pll.pll);
		goto err_free;
	}

	ret = drmm_mode_config_init(drm_dev);
	if (ret)
		goto err_free;

	rockchip_drm_mode_config_init(drm_dev);
	rockchip_drm_create_properties(drm_dev);
	/* Try to bind all sub drivers. */
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_mode_config_cleanup;

	rockchip_attach_connector_property(drm_dev);
	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm_dev);
	rockchip_drm_set_property_default(drm_dev);

#if IS_ENABLED(CONFIG_DRM_LEGACY)
	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 */
	drm_dev->irq_enabled = true;
#endif

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm_dev);

	ret = rockchip_drm_init_iommu(drm_dev);
	if (ret)
		goto err_unbind_all;

	rockchip_gem_pool_init(drm_dev);
	ret = of_reserved_mem_device_init(drm_dev->dev);
	if (ret)
		DRM_DEBUG_KMS("No reserved memory region assign to drm\n");

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_kms_helper_poll_fini;

	drm_for_each_encoder(encoder, drm_dev)
		rockchip_drm_fix_encoder_possible_clones(encoder);

	rockchip_drm_show_logo(drm_dev);

	ret = rockchip_drm_fbdev_init(drm_dev);
	if (ret)
		goto err_drm_dev_unregister;

	ret = rockchip_drm_sysfs_init(drm_dev);
	if (ret)
		goto err_drm_fbdev_fini;

	rockchip_drm_error_event_init(drm_dev);
	rockchip_clocks_loader_unprotect();

	return 0;
err_drm_fbdev_fini:
	rockchip_drm_fbdev_fini(drm_dev);
err_drm_dev_unregister:
	drm_dev_unregister(drm_dev);
err_kms_helper_poll_fini:
	rockchip_gem_pool_destroy(drm_dev);
	drm_kms_helper_poll_fini(drm_dev);
	rockchip_iommu_cleanup(drm_dev);
err_unbind_all:
	component_unbind_all(dev, drm_dev);
err_mode_config_cleanup:
	drm_mode_config_cleanup(drm_dev);
err_free:
	drm_dev->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
	return ret;
}

static void rockchip_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	rockchip_drm_error_event_fini(drm_dev);
	rockchip_drm_sysfs_fini(drm_dev);
	rockchip_drm_fbdev_fini(drm_dev);
	drm_dev_unregister(drm_dev);

	rockchip_gem_pool_destroy(drm_dev);
	drm_kms_helper_poll_fini(drm_dev);

	drm_atomic_helper_shutdown(drm_dev);
	component_unbind_all(dev, drm_dev);
	drm_mode_config_cleanup(drm_dev);
	rockchip_iommu_cleanup(drm_dev);

	drm_dev->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
}

static void rockchip_drm_crtc_cancel_pending_vblank(struct drm_crtc *crtc,
						    struct drm_file *file_priv)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (pipe < ROCKCHIP_MAX_CRTC &&
	    priv->crtc_funcs[pipe] &&
	    priv->crtc_funcs[pipe]->cancel_pending_vblank)
		priv->crtc_funcs[pipe]->cancel_pending_vblank(crtc, file_priv);
}

static int rockchip_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev)
		crtc->primary->fb = NULL;

	return 0;
}

static void rockchip_drm_postclose(struct drm_device *dev,
				   struct drm_file *file_priv)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		rockchip_drm_crtc_cancel_pending_vblank(crtc, file_priv);
}

static void rockchip_drm_lastclose(struct drm_device *dev)
{
	struct rockchip_drm_private *priv = dev->dev_private;

	if (!priv->logo)
		drm_fb_helper_restore_fbdev_mode_unlocked(priv->fbdev_helper);
}

static struct drm_pending_vblank_event *
rockchip_drm_add_vcnt_event(struct drm_crtc *crtc, union drm_wait_vblank *vblwait,
			    struct drm_file *file_priv)
{
	struct drm_pending_vblank_event *e;
	struct drm_device *dev = crtc->dev;
	unsigned long flags;

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return NULL;

	e->pipe = drm_crtc_index(crtc);
	e->event.base.type = DRM_EVENT_ROCKCHIP_CRTC_VCNT;
	e->event.base.length = sizeof(e->event.vbl);
	e->event.vbl.crtc_id = crtc->base.id;
	e->event.vbl.user_data = vblwait->request.signal;

	spin_lock_irqsave(&dev->event_lock, flags);
	drm_event_reserve_init_locked(dev, file_priv, &e->base, &e->event.base);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return e;
}

static int rockchip_drm_get_vcnt_event_ioctl(struct drm_device *dev, void *data,
					     struct drm_file *file_priv)
{
	struct rockchip_drm_private *priv = dev->dev_private;
	union drm_wait_vblank *vblwait = data;
	struct drm_pending_vblank_event *e;
	struct drm_crtc *crtc;
	unsigned int flags, pipe;

	flags = vblwait->request.type & (_DRM_VBLANK_FLAGS_MASK | _DRM_ROCKCHIP_VCNT_EVENT);
	pipe = (vblwait->request.type & _DRM_VBLANK_HIGH_CRTC_MASK);
	if (pipe)
		pipe = pipe >> _DRM_VBLANK_HIGH_CRTC_SHIFT;
	else
		pipe = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	crtc = drm_crtc_from_index(dev, pipe);

	if (flags & _DRM_ROCKCHIP_VCNT_EVENT) {
		e = rockchip_drm_add_vcnt_event(crtc, vblwait, file_priv);
		priv->vcnt[pipe].event = e;
	}

	return 0;
}

static const struct drm_ioctl_desc rockchip_ioctls[] = {
	DRM_IOCTL_DEF_DRV(ROCKCHIP_GEM_CREATE, rockchip_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ROCKCHIP_GEM_MAP_OFFSET,
			  rockchip_gem_map_offset_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ROCKCHIP_GEM_GET_PHYS, rockchip_gem_get_phys_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ROCKCHIP_GET_VCNT_EVENT, rockchip_drm_get_vcnt_event_ioctl,
			  DRM_UNLOCKED),
};

static int rockchip_drm_gem_dmabuf_begin_cpu_access(struct dma_buf *dma_buf,
						    enum dma_data_direction dir)
{
	struct drm_gem_object *obj = dma_buf->priv;

	return rockchip_gem_prime_begin_cpu_access(obj, dir);
}

static int rockchip_drm_gem_dmabuf_end_cpu_access(struct dma_buf *dma_buf,
						  enum dma_data_direction dir)
{
	struct drm_gem_object *obj = dma_buf->priv;

	return rockchip_gem_prime_end_cpu_access(obj, dir);
}

static const struct dma_buf_ops rockchip_drm_gem_prime_dmabuf_ops = {
	.cache_sgt_mapping = true,
	.attach = drm_gem_map_attach,
	.detach = drm_gem_map_detach,
	.map_dma_buf = drm_gem_map_dma_buf,
	.unmap_dma_buf = drm_gem_unmap_dma_buf,
	.release = drm_gem_dmabuf_release,
	.mmap = drm_gem_dmabuf_mmap,
	.vmap = drm_gem_dmabuf_vmap,
	.vunmap = drm_gem_dmabuf_vunmap,
	.begin_cpu_access = rockchip_drm_gem_dmabuf_begin_cpu_access,
	.end_cpu_access = rockchip_drm_gem_dmabuf_end_cpu_access,
};

static struct drm_gem_object *rockchip_drm_gem_prime_import_dev(struct drm_device *dev,
								struct dma_buf *dma_buf,
								struct device *attach_dev)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct drm_gem_object *obj;
	int ret;

	if (dma_buf->ops == &rockchip_drm_gem_prime_dmabuf_ops) {
		obj = dma_buf->priv;
		if (obj->dev == dev) {
			/*
			 * Importing dmabuf exported from out own gem increases
			 * refcount on gem itself instead of f_count of dmabuf.
			 */
			drm_gem_object_get(obj);
			return obj;
		}
	}

	if (!dev->driver->gem_prime_import_sg_table)
		return ERR_PTR(-EINVAL);

	attach = dma_buf_attach(dma_buf, attach_dev);
	if (IS_ERR(attach))
		return ERR_CAST(attach);

	get_dma_buf(dma_buf);

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto fail_detach;
	}

	obj = dev->driver->gem_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		goto fail_unmap;
	}

	obj->import_attach = attach;
	obj->resv = dma_buf->resv;

	return obj;

fail_unmap:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}

static struct drm_gem_object *rockchip_drm_gem_prime_import(struct drm_device *dev,
							    struct dma_buf *dma_buf)
{
	return rockchip_drm_gem_prime_import_dev(dev, dma_buf, dev->dev);
}

struct dma_buf *rockchip_drm_gem_prime_export(struct drm_gem_object *obj,
						     int flags)
{
	struct drm_device *dev = obj->dev;
	struct dma_buf_export_info exp_info = {
		.exp_name = KBUILD_MODNAME, /* white lie for debug */
		.owner = dev->driver->fops->owner,
		.ops = &rockchip_drm_gem_prime_dmabuf_ops,
		.size = obj->size,
		.flags = flags,
		.priv = obj,
		.resv = obj->resv,
	};

	return drm_gem_dmabuf_export(dev, &exp_info);
}

DEFINE_DRM_GEM_FOPS(rockchip_drm_driver_fops);

static const struct drm_driver rockchip_drm_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC | DRIVER_RENDER,
	.postclose		= rockchip_drm_postclose,
	.lastclose		= rockchip_drm_lastclose,
	.open			= rockchip_drm_open,
	.dumb_create		= rockchip_gem_dumb_create,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_import	= rockchip_drm_gem_prime_import,
	.gem_prime_import_sg_table	= rockchip_gem_prime_import_sg_table,
	.gem_prime_mmap		= drm_gem_prime_mmap,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init		= rockchip_drm_debugfs_init,
#endif
	.ioctls			= rockchip_ioctls,
	.num_ioctls		= ARRAY_SIZE(rockchip_ioctls),
	.fops			= &rockchip_drm_driver_fops,
	.name	= DRIVER_NAME,
	.desc	= DRIVER_DESC,
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
};

#ifdef CONFIG_PM_SLEEP
static int rockchip_drm_sys_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int rockchip_drm_sys_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static const struct dev_pm_ops rockchip_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rockchip_drm_sys_suspend,
				rockchip_drm_sys_resume)
};

#define MAX_ROCKCHIP_SUB_DRIVERS 16
static struct platform_driver *rockchip_sub_drivers[MAX_ROCKCHIP_SUB_DRIVERS];
static int num_rockchip_sub_drivers;

/*
 * Check if a vop endpoint is leading to a rockchip subdriver or bridge.
 * Should be called from the component bind stage of the drivers
 * to ensure that all subdrivers are probed.
 *
 * @ep: endpoint of a rockchip vop
 *
 * returns true if subdriver, false if external bridge and -ENODEV
 * if remote port does not contain a device.
 */
int rockchip_drm_endpoint_is_subdriver(struct device_node *ep)
{
	struct device_node *node = of_graph_get_remote_port_parent(ep);
	struct platform_device *pdev;
	struct device_driver *drv;
	int i;

	if (!node)
		return -ENODEV;

	/* status disabled will prevent creation of platform-devices */
	if (!of_device_is_available(node)) {
		of_node_put(node);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(node);
	of_node_put(node);

	/* enabled non-platform-devices can immediately return here */
	if (!pdev)
		return false;

	/*
	 * All rockchip subdrivers have probed at this point, so
	 * any device not having a driver now is an external bridge.
	 */
	drv = pdev->dev.driver;
	if (!drv) {
		platform_device_put(pdev);
		return false;
	}

	for (i = 0; i < num_rockchip_sub_drivers; i++) {
		if (rockchip_sub_drivers[i] == to_platform_driver(drv)) {
			platform_device_put(pdev);
			return true;
		}
	}

	platform_device_put(pdev);
	return false;
}

static void rockchip_drm_match_remove(struct device *dev)
{
	struct device_link *link;

	list_for_each_entry(link, &dev->links.consumers, s_node)
		device_link_del(link);
}

static struct component_match *rockchip_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < num_rockchip_sub_drivers; i++) {
		struct platform_driver *drv = rockchip_sub_drivers[i];
		struct device *p = NULL, *d;

		do {
			d = platform_find_device_by_driver(p, &drv->driver);
			put_device(p);
			p = d;

			if (!d)
				break;

			device_link_add(dev, d, DL_FLAG_STATELESS);
			component_match_add(dev, &match, component_compare_dev, d);
		} while (true);
	}

	if (IS_ERR(match))
		rockchip_drm_match_remove(dev);

	return match ?: ERR_PTR(-ENODEV);
}

static const struct component_master_ops rockchip_drm_ops = {
	.bind = rockchip_drm_bind,
	.unbind = rockchip_drm_unbind,
};

static int rockchip_drm_platform_of_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *port;
	bool found = false;
	int i;

	if (!np)
		return -ENODEV;

	for (i = 0;; i++) {
		struct device_node *iommu;

		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		iommu = of_parse_phandle(port->parent, "iommus", 0);
		if (!iommu || !of_device_is_available(iommu)) {
			DRM_DEV_DEBUG(dev,
				      "no iommu attached for %pOF, using non-iommu buffers\n",
				      port->parent);
			/*
			 * if there is a crtc not support iommu, force set all
			 * crtc use non-iommu buffer.
			 */
			is_support_iommu = false;
		}

		found = true;

		iommu_reserve_map |= of_property_read_bool(iommu, "rockchip,reserve-map");
		of_node_put(iommu);
		of_node_put(port);
	}

	if (i == 0) {
		DRM_DEV_ERROR(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!found) {
		DRM_DEV_ERROR(dev,
			      "No available vop found for display-subsystem.\n");
		return -ENODEV;
	}

	return 0;
}

static int rockchip_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;
	int ret;

	ret = rockchip_drm_platform_of_probe(dev);
#if !IS_ENABLED(CONFIG_DRM_ROCKCHIP_VKMS)
	if (ret)
		return ret;
#endif

	match = rockchip_drm_match_add(dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret)
		goto err;

	ret = component_master_add_with_match(dev, &rockchip_drm_ops, match);
	if (ret < 0)
		goto err;

	return 0;
err:
	rockchip_drm_match_remove(dev);

	return ret;
}

static int rockchip_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &rockchip_drm_ops);

	rockchip_drm_match_remove(&pdev->dev);

	return 0;
}

static void rockchip_drm_platform_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	if (drm)
		drm_atomic_helper_shutdown(drm);
}

static const struct of_device_id rockchip_drm_dt_ids[] = {
	{ .compatible = "rockchip,display-subsystem", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rockchip_drm_dt_ids);

static struct platform_driver rockchip_drm_platform_driver = {
	.probe = rockchip_drm_platform_probe,
	.remove = rockchip_drm_platform_remove,
	.shutdown = rockchip_drm_platform_shutdown,
	.driver = {
		.name = "rockchip-drm",
		.of_match_table = rockchip_drm_dt_ids,
		.pm = &rockchip_drm_pm_ops,
	},
};

#define ADD_ROCKCHIP_SUB_DRIVER(drv, cond) { \
	if (IS_ENABLED(cond) && \
	    !WARN_ON(num_rockchip_sub_drivers >= MAX_ROCKCHIP_SUB_DRIVERS)) \
		rockchip_sub_drivers[num_rockchip_sub_drivers++] = &drv; \
}

static int __init rockchip_drm_init(void)
{
	int ret;

	if (drm_firmware_drivers_only())
		return -ENODEV;

	num_rockchip_sub_drivers = 0;
#if IS_ENABLED(CONFIG_DRM_ROCKCHIP_VKMS)
	ADD_ROCKCHIP_SUB_DRIVER(rockchip_vkms_platform_driver, CONFIG_DRM_ROCKCHIP_VKMS);
#else
	ADD_ROCKCHIP_SUB_DRIVER(vop_platform_driver, CONFIG_ROCKCHIP_VOP);
	ADD_ROCKCHIP_SUB_DRIVER(vop2_platform_driver, CONFIG_ROCKCHIP_VOP2);
	ADD_ROCKCHIP_SUB_DRIVER(vconn_platform_driver, CONFIG_ROCKCHIP_VCONN);
	ADD_ROCKCHIP_SUB_DRIVER(rockchip_lvds_driver,
				CONFIG_ROCKCHIP_LVDS);
	ADD_ROCKCHIP_SUB_DRIVER(rockchip_dp_driver,
				CONFIG_ROCKCHIP_ANALOGIX_DP);
	ADD_ROCKCHIP_SUB_DRIVER(cdn_dp_driver, CONFIG_ROCKCHIP_CDN_DP);
	ADD_ROCKCHIP_SUB_DRIVER(dw_hdmi_rockchip_pltfm_driver,
				CONFIG_ROCKCHIP_DW_HDMI);
	ADD_ROCKCHIP_SUB_DRIVER(dw_mipi_dsi_rockchip_driver,
				CONFIG_ROCKCHIP_DW_MIPI_DSI);
	ADD_ROCKCHIP_SUB_DRIVER(dw_mipi_dsi2_rockchip_driver,
				CONFIG_ROCKCHIP_DW_MIPI_DSI2);
	ADD_ROCKCHIP_SUB_DRIVER(inno_hdmi_driver, CONFIG_ROCKCHIP_INNO_HDMI);
	ADD_ROCKCHIP_SUB_DRIVER(rk3066_hdmi_driver,
				CONFIG_ROCKCHIP_RK3066_HDMI);
	ADD_ROCKCHIP_SUB_DRIVER(rockchip_rgb_driver, CONFIG_ROCKCHIP_RGB);
	ADD_ROCKCHIP_SUB_DRIVER(rockchip_tve_driver, CONFIG_ROCKCHIP_DRM_TVE);
	ADD_ROCKCHIP_SUB_DRIVER(dw_dp_driver, CONFIG_ROCKCHIP_DW_DP);

#endif
	ret = platform_register_drivers(rockchip_sub_drivers,
					num_rockchip_sub_drivers);
	if (ret)
		return ret;

	ret = platform_driver_register(&rockchip_drm_platform_driver);
	if (ret)
		goto err_unreg_drivers;

	rockchip_gem_get_ddr_info();

	return 0;

err_unreg_drivers:
	platform_unregister_drivers(rockchip_sub_drivers,
				    num_rockchip_sub_drivers);
	return ret;
}

static void __exit rockchip_drm_fini(void)
{
	platform_driver_unregister(&rockchip_drm_platform_driver);

	platform_unregister_drivers(rockchip_sub_drivers,
				    num_rockchip_sub_drivers);
}

#ifdef CONFIG_VIDEO_REVERSE_IMAGE
fs_initcall(rockchip_drm_init);
#else
module_init(rockchip_drm_init);
#endif
module_exit(rockchip_drm_fini);

MODULE_AUTHOR("Mark Yao <mark.yao@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP DRM Driver");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
