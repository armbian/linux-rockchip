// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:Mark Yao <mark.yao@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fixp-arith.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/overflow.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/sort.h>

#include <drm/drm.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_writeback.h>

#ifdef CONFIG_DRM_ANALOGIX_DP
#include <drm/bridge/analogix_dp.h>
#endif

#include <soc/rockchip/rockchip_dmc.h>
#include <soc/rockchip/rockchip_system_monitor.h>
#include <soc/rockchip/rockchip-system-status.h>
#include <uapi/linux/videodev2.h>
#include "../drm_crtc_internal.h"

#include "rockchip_drm_drv.h"
#include "rockchip_drm_gem.h"
#include "rockchip_drm_fb.h"
#include "rockchip_drm_vop.h"
#include "rockchip_rgb.h"

#define VOP_REG_SUPPORT(vop, reg) \
		(reg.mask && \
		 (!reg.major || \
		  (reg.major == VOP_MAJOR(vop->version) && \
		   reg.begin_minor <= VOP_MINOR(vop->version) && \
		   reg.end_minor >= VOP_MINOR(vop->version))))

#define VOP_WIN_SUPPORT(vop, win, name) \
		VOP_REG_SUPPORT(vop, win->phy->name)

#define VOP_WIN_SCL_EXT_SUPPORT(vop, win, name) \
		(win->phy->scl->ext && \
		VOP_REG_SUPPORT(vop, win->phy->scl->ext->name))

#define VOP_CTRL_SUPPORT(vop, name) \
		VOP_REG_SUPPORT(vop, vop->data->ctrl->name)

#define VOP_INTR_SUPPORT(vop, name) \
		VOP_REG_SUPPORT(vop, vop->data->intr->name)

#define __REG_SET(x, off, mask, shift, v, write_mask, relaxed) \
		vop_mask_write(x, off, mask, shift, v, write_mask, relaxed)

#define _REG_SET(vop, name, off, reg, mask, v, relaxed) \
	do { \
		if (VOP_REG_SUPPORT(vop, reg)) \
			__REG_SET(vop, off + reg.offset, mask, reg.shift, \
				  v, reg.write_mask, relaxed); \
		else \
			dev_dbg(vop->dev, "Warning: not support "#name"\n"); \
	} while (0)

#define REG_SET(x, name, off, reg, v, relaxed) \
		_REG_SET(x, name, off, reg, reg.mask, v, relaxed)
#define REG_SET_MASK(x, name, off, reg, mask, v, relaxed) \
		_REG_SET(x, name, off, reg, reg.mask & mask, v, relaxed)

#define VOP_WIN_SET(x, win, name, v) \
		REG_SET(x, name, win->offset, VOP_WIN_NAME(win, name), v, true)
#define VOP_WIN_SET_EXT(x, win, ext, name, v) \
		REG_SET(x, name, 0, win->ext->name, v, true)
#define VOP_SCL_SET(x, win, name, v) \
		REG_SET(x, name, win->offset, win->phy->scl->name, v, true)
#define VOP_SCL_SET_EXT(x, win, name, v) \
		REG_SET(x, name, win->offset, win->phy->scl->ext->name, v, true)

#define VOP_CTRL_SET(x, name, v) \
		REG_SET(x, name, 0, (x)->data->ctrl->name, v, false)

#define VOP_CTRL_SET2(x, ctrl, name, v) \
		REG_SET(x, name, 0, ctrl->regs->name, v, false)

#define VOP_INTR_GET(vop, name) \
		vop_read_reg(vop, 0, &vop->data->ctrl->name)

#define VOP_INTR_SET(vop, name, v) \
		REG_SET(vop, name, 0, vop->data->intr->name, \
			v, false)
#define VOP_INTR_SET_MASK(vop, name, mask, v) \
		REG_SET_MASK(vop, name, 0, vop->data->intr->name, \
			     mask, v, false)

#define VOP_INTR_SET2(vop, intr, name, v) \
		REG_SET(vop, name, 0, intr->name, v, false)
#define VOP_INTR_SET_MASK2(vop, intr, name, mask, v) \
		REG_SET_MASK(vop, name, 0, intr->name, \
			     mask, v, false)

#define VOP_REG_SET(vop, group, name, v) \
		    vop_reg_set(vop, &vop->data->group->name, 0, ~0, v, #name)

#define VOP_HAS_REG(vop, group, name) \
		(!!(vop->data->group->name.mask))

#define VOP_INTR_SET_TYPE(vop, name, type, v) \
	do { \
		int i, reg = 0, mask = 0; \
		for (i = 0; i < vop->data->intr->nintrs; i++) { \
			if (vop->data->intr->intrs[i] & type) { \
				reg |= (v) << i; \
				mask |= 1 << i; \
			} \
		} \
		VOP_INTR_SET_MASK(vop, name, mask, reg); \
	} while (0)
#define VOP_INTR_GET_TYPE(vop, name, type) \
		vop_get_intr_type(vop, vop->data->intr, &vop->data->intr->name, type)

#define VOP_INTR_SET_TYPE2(vop, intr, name, type, v) \
	do { \
		int i, reg = 0, mask = 0; \
		for (i = 0; i < intr->nintrs; i++) { \
			if (intr->intrs[i] & type) { \
				reg |= (v) << i; \
				mask |= 1 << i; \
			} \
		} \
		VOP_INTR_SET_MASK2(vop, intr, name, mask, reg); \
	} while (0)
#define VOP_INTR_GET_TYPE2(vop, intr, name, type) \
		vop_get_intr_type(vop, intr, &intr->name, type)

#define VOP_CTRL_GET(x, name) \
		vop_read_reg(x, 0, &vop->data->ctrl->name)

#define VOP_CTRL_GET2(x, ctrl, name) \
		vop_read_reg(x, 0, &ctrl->regs->name)

#define VOP_WIN_GET(vop, win, name) \
		vop_read_reg(vop, win->offset, &VOP_WIN_NAME(win, name))

#define VOP_WIN_NAME(win, name) \
		(vop_get_win_phy(win, &win->phy->name)->name)

#define VOP_WIN_TO_INDEX(vop_win) \
	((vop_win) - (vop_win)->vop->win)

#define VOP_GRF_SET(vop, grf, reg, v) \
	do { \
		if (vop->data->grf) { \
			vop_grf_writel(vop->grf, vop->data->grf->reg, v); \
		} \
	} while (0)

#define to_vop_win(x) container_of(x, struct vop_win, base)
#define to_vop_plane_state(x) container_of(x, struct vop_plane_state, base)
#define to_wb_state(x) container_of(x, struct vop_wb_connector_state, base)
#define AFBC_Y2R_COLOR_TRANSFORM (1 << 4)

enum vop_pending {
	VOP_PENDING_FB_UNREF,
};

struct vop_zpos {
	int win_id;
	int zpos;
};

struct vop_plane_state {
	struct drm_plane_state base;
	int format;
	int zpos;
	struct drm_rect src;
	struct drm_rect dest;
	dma_addr_t yrgb_mst;
	dma_addr_t uv_mst;
	const uint32_t *y2r_table;
	const uint32_t *r2r_table;
	const uint32_t *r2y_table;
	int eotf;
	bool y2r_en;
	bool r2r_en;
	bool r2y_en;
	u32 color_key;
	unsigned int csc_mode;
	int global_alpha;
	int blend_mode;
	unsigned long offset;
	int pdaf_data_type;
	bool async_commit;
};

struct vop_win {
	struct vop_win *parent;
	struct drm_plane base;

	int win_id;
	int area_id;
	u8 plane_id; /* unique plane id */
	const char *name;

	int zpos;
	uint32_t offset;
	enum drm_plane_type type;
	const struct vop_win_phy *phy;
	const struct vop_csc *csc;
	const uint32_t *data_formats;
	uint32_t nformats;
	const uint64_t *format_modifiers;
	u64 feature;
	struct vop *vop;
	struct vop_plane_state state;

	struct drm_property *input_width_prop;
	struct drm_property *input_height_prop;
	struct drm_property *output_width_prop;
	struct drm_property *output_height_prop;
	struct drm_property *color_key_prop;
	struct drm_property *scale_prop;
	struct drm_property *name_prop;
};

struct vop_wb {
	struct drm_writeback_connector conn;
	const struct vop_wb_regs *regs;
};

enum vop_wb_format {
	VOP_WB_ARGB8888,
	VOP_WB_RGB888,
	VOP_WB_RGB565,
	VOP_WB_YUV420SP = 4,
	VOP_WB_INVALID = -1,
};

struct vop_wb_connector_state {
	struct drm_connector_state base;
	dma_addr_t yrgb_addr;
	dma_addr_t uv_addr;
	enum vop_wb_format format;
	bool xgt2_en;
	uint16_t scale_x_factor;
	uint8_t scale_x_en;
	uint8_t scale_y_en;
};

struct vop {
	struct rockchip_crtc rockchip_crtc;
	struct device *dev;
	struct device *genpd_dev0;
	struct device *genpd_dev1;
	struct drm_device *drm_dev;
	struct dentry *debugfs;
	struct drm_info_list *debugfs_files;
	struct drm_property *plane_feature_prop;
	struct drm_property *feature_prop;

	bool is_iommu_enabled;
	bool is_enabled;
	bool support_multi_area;

	bool aclk_rate_reset;
	unsigned long aclk_rate;

	/**
	 * @enabled_win_mask: Bitmask of enabled wins attached to the VOP
	 */
	uint32_t enabled_win_mask;

	u32 version;
	u32 background;
	u32 line_flag;
	u8 id;
	u64 soc_id;
	struct drm_prop_enum_list *plane_name_list;

	struct drm_tv_connector_state active_tv_state;
	bool pre_overlay;
	bool loader_protect;
	struct completion dsp_hold_completion;

	/* protected by dev->event_lock */
	struct drm_pending_vblank_event *event;

	struct drm_flip_work fb_unref_work;
	unsigned long pending;

	struct completion line_flag_completion;

	const struct vop_data *data;
	int num_wins;

	uint32_t *regsbak;
	struct resource *res;
	void __iomem *regs;
	struct regmap *grf;
	struct regmap *vo0_grf;

	/* physical map length of vop register */
	uint32_t len;

	struct resource *lut_res;
	void __iomem *lut_regs;
	u32 *lut;
	u32 lut_len;
	bool lut_active;
	/* gamma look up table */
	struct drm_color_lut *gamma_lut;
	bool dual_channel_swap;
	/* one time only one process allowed to config the register */
	spinlock_t reg_lock;
	/* lock vop irq reg */
	spinlock_t irq_lock;
	/* protects crtc enable/disable */
	struct mutex vop_lock;

	unsigned int irq;

	/* vop AHP clk */
	struct clk *hclk;
	/* vop dclk */
	struct clk *dclk;
	/* vop share memory frequency */
	struct clk *aclk;
	/* vop source handling, optional */
	struct clk *dclk_source;

	/* vop dclk reset */
	struct reset_control *dclk_rst;

	struct rockchip_dclk_pll *pll;

	struct rockchip_mcu_timing mcu_timing;
	struct rockchip_mcu_timing mcu_bypass_timing;

	/**
	 * @wb_en: write back enabled
	 */
	bool wb_en;
	struct vop_wb wb;

	struct vop_win win[];
};

/*
 * bus-format types.
 */
struct drm_bus_format_enum_list {
	int type;
	const char *name;
};

static const struct drm_bus_format_enum_list drm_bus_format_enum_list[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "Unknown" },
	{ MEDIA_BUS_FMT_RGB565_1X16, "RGB565_1X16" },
	{ MEDIA_BUS_FMT_RGB666_1X18, "RGB666_1X18" },
	{ MEDIA_BUS_FMT_RGB666_1X24_CPADHI, "RGB666_1X24_CPADHI" },
	{ MEDIA_BUS_FMT_RGB666_1X7X3_SPWG, "RGB666_1X7X3_SPWG" },
	{ MEDIA_BUS_FMT_YUV8_1X24, "YUV8_1X24" },
	{ MEDIA_BUS_FMT_UYYVYY8_0_5X24, "UYYVYY8_0_5X24" },
	{ MEDIA_BUS_FMT_YUV10_1X30, "YUV10_1X30" },
	{ MEDIA_BUS_FMT_UYYVYY10_0_5X30, "UYYVYY10_0_5X30" },
	{ MEDIA_BUS_FMT_RGB565_2X8_LE, "RGB565_2X8_LE" },
	{ MEDIA_BUS_FMT_RGB666_3X6, "RGB666_3X6" },
	{ MEDIA_BUS_FMT_RGB888_3X8, "RGB888_3X8" },
	{ MEDIA_BUS_FMT_RGB888_DUMMY_4X8, "RGB888_DUMMY_4X8" },
	{ MEDIA_BUS_FMT_RGB888_1X24, "RGB888_1X24" },
	{ MEDIA_BUS_FMT_RGB888_1X7X4_SPWG, "RGB888_1X7X4_SPWG" },
	{ MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA, "RGB888_1X7X4_JEIDA" },
	{ MEDIA_BUS_FMT_UYVY8_2X8, "UYVY8_2X8" },
	{ MEDIA_BUS_FMT_YUYV8_1X16, "YUYV8_1X16" },
	{ MEDIA_BUS_FMT_UYVY8_1X16, "UYVY8_1X16" },
};

static DRM_ENUM_NAME_FN(drm_get_bus_format_name, drm_bus_format_enum_list)

static inline struct vop *to_vop(struct drm_crtc *crtc)
{
	struct rockchip_crtc *rockchip_crtc;

	rockchip_crtc = container_of(crtc, struct rockchip_crtc, crtc);

	return container_of(rockchip_crtc, struct vop, rockchip_crtc);
}

static void vop_lock(struct vop *vop)
{
	mutex_lock(&vop->vop_lock);
	rockchip_dmcfreq_lock();
}

static void vop_unlock(struct vop *vop)
{
	rockchip_dmcfreq_unlock();
	mutex_unlock(&vop->vop_lock);
}

static inline void vop_grf_writel(struct regmap *regmap, struct vop_reg reg, u32 v)
{
	u32 val = 0;

	if (IS_ERR_OR_NULL(regmap))
		return;

	if (reg.mask) {
		val = (v << reg.shift) | (reg.mask << (reg.shift + 16));
		regmap_write(regmap, reg.offset, val);
	}
}

static inline void vop_writel(struct vop *vop, uint32_t offset, uint32_t v)
{
	writel(v, vop->regs + offset);
	vop->regsbak[offset >> 2] = v;
}

static inline uint32_t vop_readl(struct vop *vop, uint32_t offset)
{
	return readl(vop->regs + offset);
}

static inline uint32_t vop_read_reg(struct vop *vop, uint32_t base,
				    const struct vop_reg *reg)
{
	return (vop_readl(vop, base + reg->offset) >> reg->shift) & reg->mask;
}

static inline void vop_write_reg_uncached(struct vop *vop, const struct vop_reg *reg, uint32_t v)
{
	uint32_t offset = reg->offset;
	uint32_t cached_val = vop->regsbak[offset >> 2];

	v = (cached_val & ~(reg->mask << reg->shift)) | ((v & reg->mask) << reg->shift);
	writel(v, vop->regs + offset);
}

static inline void vop_mask_write(struct vop *vop, uint32_t offset,
				  uint32_t mask, uint32_t shift, uint32_t v,
				  bool write_mask, bool relaxed)
{
	if (!mask)
		return;

	if (write_mask) {
		v = ((v & mask) << shift) | (mask << (shift + 16));
	} else {
		uint32_t cached_val = vop->regsbak[offset >> 2];

		v = (cached_val & ~(mask << shift)) | ((v & mask) << shift);
		vop->regsbak[offset >> 2] = v;
	}

	if (relaxed)
		writel_relaxed(v, vop->regs + offset);
	else
		writel(v, vop->regs + offset);
}

static inline const struct vop_win_phy *
vop_get_win_phy(struct vop_win *win, const struct vop_reg *reg)
{
	if (!reg->mask && win->parent)
		return win->parent->phy;

	return win->phy;
}

static inline uint32_t vop_get_intr_type(struct vop *vop, const struct vop_intr *intr,
					 const struct vop_reg *reg, int type)
{
	uint32_t i, ret = 0;
	uint32_t regs = vop_read_reg(vop, 0, reg);

	for (i = 0; i < intr->nintrs; i++) {
		if ((type & intr->intrs[i]) && (regs & 1 << i))
			ret |= intr->intrs[i];
	}

	return ret;
}

static void vop_load_hdr2sdr_table(struct vop *vop)
{
	int i;
	const struct vop_hdr_table *table = vop->data->hdr_table;
	uint32_t hdr2sdr_eetf_oetf_yn[33];

	for (i = 0; i < 33; i++)
		hdr2sdr_eetf_oetf_yn[i] = table->hdr2sdr_eetf_yn[i] +
				(table->hdr2sdr_bt1886oetf_yn[i] << 16);

	vop_writel(vop, table->hdr2sdr_eetf_oetf_y0_offset,
		   hdr2sdr_eetf_oetf_yn[0]);
	for (i = 1; i < 33; i++)
		vop_writel(vop,
			   table->hdr2sdr_eetf_oetf_y1_offset + (i - 1) * 4,
			   hdr2sdr_eetf_oetf_yn[i]);

	vop_writel(vop, table->hdr2sdr_sat_y0_offset,
		   table->hdr2sdr_sat_yn[0]);
	for (i = 1; i < 9; i++)
		vop_writel(vop, table->hdr2sdr_sat_y1_offset + (i - 1) * 4,
			   table->hdr2sdr_sat_yn[i]);

	VOP_CTRL_SET(vop, hdr2sdr_src_min, table->hdr2sdr_src_range_min);
	VOP_CTRL_SET(vop, hdr2sdr_src_max, table->hdr2sdr_src_range_max);
	VOP_CTRL_SET(vop, hdr2sdr_normfaceetf, table->hdr2sdr_normfaceetf);
	VOP_CTRL_SET(vop, hdr2sdr_dst_min, table->hdr2sdr_dst_range_min);
	VOP_CTRL_SET(vop, hdr2sdr_dst_max, table->hdr2sdr_dst_range_max);
	VOP_CTRL_SET(vop, hdr2sdr_normfacgamma, table->hdr2sdr_normfacgamma);
}

static void vop_load_sdr2hdr_table(struct vop *vop, uint32_t cmd)
{
	int i;
	const struct vop_hdr_table *table = vop->data->hdr_table;
	uint32_t sdr2hdr_eotf_oetf_yn[65];
	uint32_t sdr2hdr_oetf_dx_dxpow[64];

	if (cmd != SDR2HDR_FOR_BT2020 && cmd != SDR2HDR_FOR_HDR && cmd != SDR2HDR_FOR_HLG_HDR) {
		DRM_WARN("unknown sdr2hdr oetf: %d\n", cmd);
		return;
	}

	for (i = 0; i < 65; i++) {
		if (cmd == SDR2HDR_FOR_BT2020)
			sdr2hdr_eotf_oetf_yn[i] =
				table->sdr2hdr_bt1886eotf_yn_for_bt2020[i] +
				(table->sdr2hdr_st2084oetf_yn_for_bt2020[i] << 18);
		else if (cmd == SDR2HDR_FOR_HDR)
			sdr2hdr_eotf_oetf_yn[i] =
				table->sdr2hdr_bt1886eotf_yn_for_hdr[i] +
				(table->sdr2hdr_st2084oetf_yn_for_hdr[i] << 18);
		else if (cmd == SDR2HDR_FOR_HLG_HDR)
			sdr2hdr_eotf_oetf_yn[i] =
				table->sdr2hdr_bt1886eotf_yn_for_hlg_hdr[i] +
				(table->sdr2hdr_st2084oetf_yn_for_hlg_hdr[i] << 18);
	}
	vop_writel(vop, table->sdr2hdr_eotf_oetf_y0_offset,
		   sdr2hdr_eotf_oetf_yn[0]);
	for (i = 1; i < 65; i++)
		vop_writel(vop, table->sdr2hdr_eotf_oetf_y1_offset +
			   (i - 1) * 4, sdr2hdr_eotf_oetf_yn[i]);

	for (i = 0; i < 64; i++) {
		sdr2hdr_oetf_dx_dxpow[i] = table->sdr2hdr_st2084oetf_dxn[i] +
				(table->sdr2hdr_st2084oetf_dxn_pow2[i] << 16);
		vop_writel(vop, table->sdr2hdr_oetf_dx_dxpow1_offset + i * 4,
			   sdr2hdr_oetf_dx_dxpow[i]);
	}

	for (i = 0; i < 63; i++)
		vop_writel(vop, table->sdr2hdr_oetf_xn1_offset + i * 4,
			   table->sdr2hdr_st2084oetf_xn[i]);
}

static void vop_load_csc_table(struct vop *vop, u32 offset, const u32 *table)
{
	int i;

	/*
	 * so far the csc offset is not 0 and in the feature the csc offset
	 * impossible be 0, so when the offset is 0, should return here.
	 */
	if (!table || offset == 0)
		return;

	for (i = 0; i < 8; i++)
		vop_writel(vop, offset + i * 4, table[i]);
}

static inline void vop_cfg_done(struct vop *vop)
{
	VOP_CTRL_SET(vop, cfg_done, 1);
}

static inline void vop_wb_cfg_done(struct vop *vop)
{
	struct vop_wb *wb = &vop->wb;

	VOP_CTRL_SET2(vop, wb, cfg_done, 1);
}

static bool vop_is_allwin_disabled(struct vop *vop)
{
	int i;

	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *win = &vop->win[i];

		if (VOP_WIN_GET(vop, win, enable) != 0)
			return false;
	}

	return true;
}

static void vop_win_disable(struct vop *vop, struct vop_win *win)
{
	vop->enabled_win_mask &= ~BIT(win->win_id);
	/*
	 * FIXUP: some of the vop scale would be abnormal after windows power
	 * on/off so deinit scale to scale_none mode.
	 */
	if (win->phy->scl && win->phy->scl->ext) {
		VOP_SCL_SET_EXT(vop, win, yrgb_hor_scl_mode, SCALE_NONE);
		VOP_SCL_SET_EXT(vop, win, yrgb_ver_scl_mode, SCALE_NONE);
		VOP_SCL_SET_EXT(vop, win, cbcr_hor_scl_mode, SCALE_NONE);
		VOP_SCL_SET_EXT(vop, win, cbcr_ver_scl_mode, SCALE_NONE);
	}

	VOP_WIN_SET(vop, win, enable, 0);
	if (win->area_id == 0)
		VOP_WIN_SET(vop, win, gate, 0);
}

static void vop_disable_allwin(struct vop *vop)
{
	int i;

	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *win = &vop->win[i];

		vop_win_disable(vop, win);
	}
}

static inline void vop_write_lut(struct vop *vop, uint32_t offset, uint32_t v)
{
	writel(v, vop->lut_regs + offset);
}

static inline uint32_t vop_read_lut(struct vop *vop, uint32_t offset)
{
	return readl(vop->lut_regs + offset);
}

static bool has_rb_swapped(uint32_t version, uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGR565:
		return true;
	/*
	 * full framework (IP version 3.x) only need rb swapped for RGB888 and
	 * little framework (IP version 2.x) only need rb swapped for BGR888,
	 * check for 3.x to also only rb swap BGR888 for unknown vop version
	 */
	case DRM_FORMAT_RGB888:
		return VOP_MAJOR(version) == 3;
	case DRM_FORMAT_BGR888:
		return VOP_MAJOR(version) != 3;
	default:
		return false;
	}
}

static bool has_uv_swapped(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV42:
		return true;
	default:
		return false;
	}
}

static enum vop_data_format vop_convert_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return VOP_FMT_ARGB8888;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		return VOP_FMT_RGB888;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		return VOP_FMT_RGB565;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV15:
		return VOP_FMT_YUV420SP;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV20:
		return VOP_FMT_YUV422SP;
	case DRM_FORMAT_NV24:
	case DRM_FORMAT_NV42:
	case DRM_FORMAT_NV30:
		return VOP_FMT_YUV444SP;
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
		return VOP_FMT_YUYV;
	default:
		DRM_ERROR("unsupported format[%08x]\n", format);
		return -EINVAL;
	}
}

static int vop_convert_afbc_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return AFBDC_FMT_U8U8U8U8;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		return AFBDC_FMT_U8U8U8;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		return AFBDC_FMT_RGB565;
	/* either of the below should not be reachable */
	default:
		DRM_WARN_ONCE("unsupported AFBC format[%08x]\n", format);
		return -EINVAL;
	}
}

static bool is_uv_swap(uint32_t bus_format, uint32_t output_mode)
{
	/*
	 * FIXME:
	 *
	 * There is no media type for YUV444 output,
	 * so when out_mode is AAAA or P888, assume output is YUV444 on
	 * yuv format.
	 *
	 * From H/W testing, YUV444 mode need a rb swap.
	 */
	if (bus_format == MEDIA_BUS_FMT_YVYU8_1X16 ||
	    bus_format == MEDIA_BUS_FMT_VYUY8_1X16 ||
	    bus_format == MEDIA_BUS_FMT_YVYU8_2X8 ||
	    bus_format == MEDIA_BUS_FMT_VYUY8_2X8 ||
	    ((bus_format == MEDIA_BUS_FMT_YUV8_1X24 ||
	      bus_format == MEDIA_BUS_FMT_YUV10_1X30) &&
	     (output_mode == ROCKCHIP_OUT_MODE_AAAA ||
	      output_mode == ROCKCHIP_OUT_MODE_P888)))
		return true;
	else
		return false;
}

static bool is_rb_swap(uint32_t bus_format, uint32_t output_mode)
{
	/*
	 * The default component order of serial formats
	 * is BGR. So it is needed to enable RB swap.
	 */
	if (bus_format == MEDIA_BUS_FMT_RGB888_3X8 ||
	    bus_format == MEDIA_BUS_FMT_RGB888_DUMMY_4X8 ||
	    bus_format == MEDIA_BUS_FMT_RGB666_3X6 ||
	    bus_format == MEDIA_BUS_FMT_RGB565_2X8_LE)
		return true;
	else
		return false;
}

static bool is_yc_swap(uint32_t bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
		return true;
	default:
		return false;
	}
}

static bool is_yuv_output(uint32_t bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
		return true;
	default:
		return false;
	}
}

static bool is_yuv_support(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV15:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV20:
	case DRM_FORMAT_NV24:
	case DRM_FORMAT_NV30:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
		return true;
	default:
		return false;
	}
}

static bool is_yuyv_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
		return true;
	default:
		return false;
	}
}

static bool is_yuv_10bit(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_NV15:
	case DRM_FORMAT_NV20:
	case DRM_FORMAT_NV30:
		return true;
	default:
		return false;
	}
}

static bool is_alpha_support(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
		return true;
	default:
		return false;
	}
}

static inline bool rockchip_afbc(struct drm_plane *plane, u64 modifier)
{
	return rockchip_drm_is_afbc(plane, modifier);
}

static enum vop_wb_format vop_convert_wb_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_ARGB8888:
		return VOP_WB_ARGB8888;
	case DRM_FORMAT_RGB888:
		return VOP_WB_RGB888;
	case DRM_FORMAT_RGB565:
		return VOP_WB_RGB565;
	case DRM_FORMAT_NV12:
		return VOP_WB_YUV420SP;
	default:
		DRM_ERROR("unsupported wb format[%p4cc]\n", &format);
		return VOP_WB_INVALID;
	}
}

static uint16_t scl_vop_cal_scale(enum scale_mode mode, uint32_t src,
				  uint32_t dst, bool is_horizontal,
				  int vsu_mode, int *vskiplines)
{
	uint16_t val = 1 << SCL_FT_DEFAULT_FIXPOINT_SHIFT;

	if (vskiplines)
		*vskiplines = 0;

	if (is_horizontal) {
		if (mode == SCALE_UP)
			val = GET_SCL_FT_BIC(src, dst);
		else if (mode == SCALE_DOWN)
			val = GET_SCL_FT_BILI_DN(src, dst);
	} else {
		if (mode == SCALE_UP) {
			if (vsu_mode == SCALE_UP_BIL)
				val = GET_SCL_FT_BILI_UP(src, dst);
			else
				val = GET_SCL_FT_BIC(src, dst);
		} else if (mode == SCALE_DOWN) {
			if (vskiplines) {
				*vskiplines = scl_get_vskiplines(src, dst);
				val = scl_get_bili_dn_vskip(src, dst,
							    *vskiplines);
			} else {
				val = GET_SCL_FT_BILI_DN(src, dst);
			}
		}
	}

	return val;
}

static void scl_vop_cal_scl_fac(struct vop *vop, const struct vop_win *win,
				uint32_t src_w, uint32_t src_h, uint32_t dst_w,
				uint32_t dst_h, uint32_t pixel_format)
{
	uint16_t yrgb_hor_scl_mode, yrgb_ver_scl_mode;
	uint16_t cbcr_hor_scl_mode = SCALE_NONE;
	uint16_t cbcr_ver_scl_mode = SCALE_NONE;
	const struct drm_format_info *info = drm_format_info(pixel_format);
	uint8_t hsub = info->hsub;
	uint8_t vsub = info->vsub;
	bool is_yuv = false;
	uint16_t cbcr_src_w = src_w / hsub;
	uint16_t cbcr_src_h = src_h / vsub;
	uint16_t vsu_mode;
	uint16_t lb_mode;
	uint32_t val;
	const struct vop_data *vop_data = vop->data;
	struct drm_display_mode *adjusted_mode = &vop->rockchip_crtc.crtc.state->adjusted_mode;
	int vskiplines;

	if (!win->phy->scl)
		return;

	if ((adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) &&
	    vop->version == VOP_VERSION_RK3036) {
		VOP_SCL_SET(vop, win, scale_yrgb_x, ((src_w << 12) / dst_w));
		VOP_SCL_SET(vop, win, scale_yrgb_y, ((src_h << 12) / dst_h));
		if (is_yuv) {
			VOP_SCL_SET(vop, win, scale_cbcr_x, ((cbcr_src_w << 12) / dst_w));
			VOP_SCL_SET(vop, win, scale_cbcr_y, ((cbcr_src_h << 12) / dst_h));
		}
		return;
	}

	if (!(vop_data->feature & VOP_FEATURE_ALPHA_SCALE)) {
		if (is_alpha_support(pixel_format) &&
		    (src_w != dst_w || src_h != dst_h))
			DRM_ERROR("ERROR: unsupported ppixel alpha&scale\n");
	}

	if (info->is_yuv)
		is_yuv = true;

	if (!win->phy->scl->ext) {
		VOP_SCL_SET(vop, win, scale_yrgb_x,
			    scl_cal_scale2(src_w, dst_w));
		VOP_SCL_SET(vop, win, scale_yrgb_y,
			    scl_cal_scale2(src_h, dst_h));
		if (is_yuv) {
			VOP_SCL_SET(vop, win, scale_cbcr_x,
				    scl_cal_scale2(cbcr_src_w, dst_w));
			VOP_SCL_SET(vop, win, scale_cbcr_y,
				    scl_cal_scale2(cbcr_src_h, dst_h));
		}
		return;
	}

	yrgb_hor_scl_mode = scl_get_scl_mode(src_w, dst_w);
	yrgb_ver_scl_mode = scl_get_scl_mode(src_h, dst_h);

	if (is_yuv) {
		cbcr_hor_scl_mode = scl_get_scl_mode(cbcr_src_w, dst_w);
		cbcr_ver_scl_mode = scl_get_scl_mode(cbcr_src_h, dst_h);
		if (cbcr_hor_scl_mode == SCALE_DOWN)
			lb_mode = scl_vop_cal_lb_mode(dst_w, true);
		else
			lb_mode = scl_vop_cal_lb_mode(cbcr_src_w, true);
	} else {
		if (yrgb_hor_scl_mode == SCALE_DOWN)
			lb_mode = scl_vop_cal_lb_mode(dst_w, false);
		else
			lb_mode = scl_vop_cal_lb_mode(src_w, false);
	}

	VOP_SCL_SET_EXT(vop, win, lb_mode, lb_mode);
	if (lb_mode == LB_RGB_3840X2) {
		if (yrgb_ver_scl_mode != SCALE_NONE) {
			DRM_DEV_ERROR(vop->dev, "not allow yrgb ver scale\n");
			return;
		}
		if (cbcr_ver_scl_mode != SCALE_NONE) {
			DRM_DEV_ERROR(vop->dev, "not allow cbcr ver scale\n");
			return;
		}
		vsu_mode = SCALE_UP_BIL;
	} else if (lb_mode == LB_RGB_2560X4) {
		vsu_mode = SCALE_UP_BIL;
	} else {
		vsu_mode = SCALE_UP_BIC;
	}

	val = scl_vop_cal_scale(yrgb_hor_scl_mode, src_w, dst_w,
				true, 0, NULL);
	VOP_SCL_SET(vop, win, scale_yrgb_x, val);
	val = scl_vop_cal_scale(yrgb_ver_scl_mode, src_h, dst_h,
				false, vsu_mode, &vskiplines);
	VOP_SCL_SET(vop, win, scale_yrgb_y, val);

	VOP_SCL_SET_EXT(vop, win, vsd_yrgb_gt4, vskiplines == 4);
	VOP_SCL_SET_EXT(vop, win, vsd_yrgb_gt2, vskiplines == 2);

	VOP_SCL_SET_EXT(vop, win, yrgb_hor_scl_mode, yrgb_hor_scl_mode);
	VOP_SCL_SET_EXT(vop, win, yrgb_ver_scl_mode, yrgb_ver_scl_mode);
	VOP_SCL_SET_EXT(vop, win, yrgb_hsd_mode, SCALE_DOWN_BIL);
	VOP_SCL_SET_EXT(vop, win, yrgb_vsd_mode, SCALE_DOWN_BIL);
	VOP_SCL_SET_EXT(vop, win, yrgb_vsu_mode, vsu_mode);
	if (is_yuv) {
		val = scl_vop_cal_scale(cbcr_hor_scl_mode, cbcr_src_w,
					dst_w, true, 0, NULL);
		VOP_SCL_SET(vop, win, scale_cbcr_x, val);
		val = scl_vop_cal_scale(cbcr_ver_scl_mode, cbcr_src_h,
					dst_h, false, vsu_mode, &vskiplines);
		VOP_SCL_SET(vop, win, scale_cbcr_y, val);

		VOP_SCL_SET_EXT(vop, win, vsd_cbcr_gt4, vskiplines == 4);
		VOP_SCL_SET_EXT(vop, win, vsd_cbcr_gt2, vskiplines == 2);
		VOP_SCL_SET_EXT(vop, win, cbcr_hor_scl_mode, cbcr_hor_scl_mode);
		VOP_SCL_SET_EXT(vop, win, cbcr_ver_scl_mode, cbcr_ver_scl_mode);
		VOP_SCL_SET_EXT(vop, win, cbcr_hsd_mode, SCALE_DOWN_BIL);
		VOP_SCL_SET_EXT(vop, win, cbcr_vsd_mode, SCALE_DOWN_BIL);
		VOP_SCL_SET_EXT(vop, win, cbcr_vsu_mode, vsu_mode);
	}
}

/*
 * rk3328 HDR/CSC path
 *
 * HDR/SDR --> win0  --> HDR2SDR ----\
 *		  \		      MUX --\
 *                 \ --> SDR2HDR/CSC--/      \
 *                                            \
 * SDR --> win1 -->pre_overlay ->SDR2HDR/CSC --> post_ovrlay-->post CSC-->output
 * SDR --> win2 -/
 *
 */

static int vop_hdr_atomic_check(struct drm_crtc *crtc,
				struct drm_crtc_state *crtc_state)
{
	struct drm_atomic_state *state = crtc_state->state;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_plane_state *pstate;
	struct drm_plane *plane;
	struct vop *vop = to_vop(crtc);
	int pre_sdr2hdr_state = 0, post_sdr2hdr_state = 0;
	int pre_sdr2hdr_mode = 0, post_sdr2hdr_mode = 0, sdr2hdr_func = 0;
	bool pre_overlay = false;
	int hdr2sdr_en = 0, plane_id = 0;

	if (!vop->data->hdr_table)
		return 0;
	/* hdr cover */
	drm_atomic_crtc_state_for_each_plane(plane, crtc_state) {
		struct vop_plane_state *vop_plane_state;
		struct vop_win *win = to_vop_win(plane);

		pstate = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(pstate))
			return PTR_ERR(pstate);
		vop_plane_state = to_vop_plane_state(pstate);
		if (!pstate->fb)
			continue;

		if (vop_plane_state->eotf > s->eotf)
			if (win->feature & WIN_FEATURE_HDR2SDR)
				hdr2sdr_en = 1;
		if (vop_plane_state->eotf < s->eotf) {
			if (win->feature & WIN_FEATURE_PRE_OVERLAY)
				pre_sdr2hdr_state |= BIT(plane_id);
			else
				post_sdr2hdr_state |= BIT(plane_id);
		}
		plane_id++;
	}

	if (pre_sdr2hdr_state || post_sdr2hdr_state || hdr2sdr_en) {
		pre_overlay = true;
		pre_sdr2hdr_mode = BT709_TO_BT2020;
		post_sdr2hdr_mode = BT709_TO_BT2020;
		sdr2hdr_func = SDR2HDR_FOR_HDR;
		goto exit_hdr_convert;
	}

	/* overlay mode */
	plane_id = 0;
	pre_overlay = false;
	pre_sdr2hdr_mode = 0;
	post_sdr2hdr_mode = 0;
	pre_sdr2hdr_state = 0;
	post_sdr2hdr_state = 0;
	drm_atomic_crtc_state_for_each_plane(plane, crtc_state) {
		struct vop_win *win = to_vop_win(plane);

		pstate = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(pstate))
			return PTR_ERR(pstate);
		if (!pstate->fb)
			continue;

		/* bt.2020 to bt.709 */
		if (pstate->color_encoding == DRM_COLOR_YCBCR_BT2020 &&
		    s->color_encoding != DRM_COLOR_YCBCR_BT2020) {
			if (win->feature & WIN_FEATURE_PRE_OVERLAY) {
				pre_sdr2hdr_mode = BT2020_TO_BT709;
				pre_sdr2hdr_state |= BIT(plane_id);
			} else {
				post_sdr2hdr_mode = BT2020_TO_BT709;
				post_sdr2hdr_state |= BIT(plane_id);
			}
		}

		/* bt.709 to bt.2020 */
		if (s->color_encoding == DRM_COLOR_YCBCR_BT2020 &&
		    pstate->color_encoding != DRM_COLOR_YCBCR_BT2020) {
			if (win->feature & WIN_FEATURE_PRE_OVERLAY) {
				pre_sdr2hdr_mode = BT709_TO_BT2020;
				pre_sdr2hdr_state |= BIT(plane_id);
			} else {
				post_sdr2hdr_mode = BT709_TO_BT2020;
				post_sdr2hdr_state |= BIT(plane_id);
			}
		}
		plane_id++;
	}

	if (pre_sdr2hdr_state || post_sdr2hdr_state) {
		pre_overlay = true;
		sdr2hdr_func = SDR2HDR_FOR_BT2020;
	}

exit_hdr_convert:
	s->hdr.pre_overlay = pre_overlay;
	s->hdr.hdr2sdr_en = hdr2sdr_en;
	if (s->hdr.pre_overlay)
		s->yuv_overlay = 0;

	s->hdr.sdr2hdr_state.bt1886eotf_pre_conv_en = !!pre_sdr2hdr_state;
	s->hdr.sdr2hdr_state.rgb2rgb_pre_conv_en = !!pre_sdr2hdr_state;
	s->hdr.sdr2hdr_state.rgb2rgb_pre_conv_mode = pre_sdr2hdr_mode;
	s->hdr.sdr2hdr_state.st2084oetf_pre_conv_en = !!pre_sdr2hdr_state;

	s->hdr.sdr2hdr_state.bt1886eotf_post_conv_en = !!post_sdr2hdr_state;
	s->hdr.sdr2hdr_state.rgb2rgb_post_conv_en = !!post_sdr2hdr_state;
	s->hdr.sdr2hdr_state.rgb2rgb_post_conv_mode = post_sdr2hdr_mode;
	s->hdr.sdr2hdr_state.st2084oetf_post_conv_en = !!post_sdr2hdr_state;
	s->hdr.sdr2hdr_state.sdr2hdr_func = sdr2hdr_func;

	return 0;
}

static enum vop_csc_format to_vop_csc_mode(enum drm_color_encoding color_encoding,
					   enum drm_color_range color_range)
{
	bool full_range = color_range == DRM_COLOR_YCBCR_FULL_RANGE ? 1 : 0;
	enum vop_csc_format csc_mode = CSC_BT709L;

	switch (color_encoding) {
	case DRM_COLOR_YCBCR_BT601:
		if (full_range)
			csc_mode = CSC_BT601F;
		else
			csc_mode = CSC_BT601L;
		break;

	case DRM_COLOR_YCBCR_BT709:
		if (full_range) {
			csc_mode = CSC_BT601F;
			DRM_DEBUG("Unsupported bt709f at 10bit csc depth, use bt601f instead\n");
		} else {
			csc_mode = CSC_BT709L;
		}
		break;

	case DRM_COLOR_YCBCR_BT2020:
		if (full_range) {
			csc_mode = CSC_BT601F;
			DRM_DEBUG("Unsupported bt2020f at 10bit csc depth, use bt601f instead\n");
		} else {
			csc_mode = CSC_BT2020L;
		}
		break;

	default:
		DRM_ERROR("Unsuport color_encoding:%d\n", color_encoding);
	}

	return csc_mode;
}

static void vop_disable_all_planes(struct vop *vop)
{
	bool active;
	int ret;

	vop_disable_allwin(vop);
	vop_cfg_done(vop);
	ret = readx_poll_timeout_atomic(vop_is_allwin_disabled,
					vop, active, active,
					0, 500 * 1000);
	if (ret)
		dev_err(vop->dev, "wait win close timeout\n");
}

/*
 * rk3399 colorspace path:
 *      Input        Win csc                     Output
 * 1. YUV(2020)  --> Y2R->2020To709->R2Y   --> YUV_OUTPUT(601/709)
 *    RGB        --> R2Y                  __/
 *
 * 2. YUV(2020)  --> bypasss               --> YUV_OUTPUT(2020)
 *    RGB        --> 709To2020->R2Y       __/
 *
 * 3. YUV(2020)  --> Y2R->2020To709        --> RGB_OUTPUT(709)
 *    RGB        --> R2Y                  __/
 *
 * 4. YUV(601/709)-> Y2R->709To2020->R2Y   --> YUV_OUTPUT(2020)
 *    RGB        --> 709To2020->R2Y       __/
 *
 * 5. YUV(601/709)-> bypass                --> YUV_OUTPUT(709)
 *    RGB        --> R2Y                  __/
 *
 * 6. YUV(601/709)-> bypass                --> YUV_OUTPUT(601)
 *    RGB        --> R2Y(601)             __/
 *
 * 7. YUV        --> Y2R(709)              --> RGB_OUTPUT(709)
 *    RGB        --> bypass               __/
 *
 * 8. RGB        --> 709To2020->R2Y        --> YUV_OUTPUT(2020)
 *
 * 9. RGB        --> R2Y(709)              --> YUV_OUTPUT(709)
 *
 * 10. RGB       --> R2Y(601)              --> YUV_OUTPUT(601)
 *
 * 11. RGB       --> bypass                --> RGB_OUTPUT(709)
 */
static int vop_setup_csc_table(struct rockchip_crtc_state *s,
			       struct drm_plane_state *pstate, const struct vop_csc_table *csc_table,
			       const uint32_t **y2r_table,
			       const uint32_t **r2r_table,
			       const uint32_t **r2y_table)
{
	bool is_input_yuv = is_yuv_support(pstate->fb->format->format);
	bool is_output_yuv = is_yuv_output(s->bus_format);
	int input_csc = pstate->color_encoding;
	int output_csc = s->color_encoding;

	*y2r_table = NULL;
	*r2r_table = NULL;
	*r2y_table = NULL;

	if (!csc_table)
		return 0;

	if (is_output_yuv) {
		if (output_csc == DRM_COLOR_YCBCR_BT2020) {
			if (is_input_yuv) {
				if (input_csc == DRM_COLOR_YCBCR_BT2020)
					return 0;
				*y2r_table = csc_table->y2r_bt709;
			}
			if (input_csc != DRM_COLOR_YCBCR_BT2020)
				*r2r_table = csc_table->r2r_bt709_to_bt2020;
			*r2y_table = csc_table->r2y_bt2020;
		} else {
			if (is_input_yuv && input_csc == DRM_COLOR_YCBCR_BT2020)
				*y2r_table = csc_table->y2r_bt2020;
			if (input_csc == DRM_COLOR_YCBCR_BT2020)
				*r2r_table = csc_table->r2r_bt2020_to_bt709;
			if (!is_input_yuv || *y2r_table) {
				if (output_csc == DRM_COLOR_YCBCR_BT709)
					*r2y_table = csc_table->r2y_bt709;
				else if (output_csc == DRM_COLOR_YCBCR_BT601 &&
					 s->color_range == DRM_COLOR_YCBCR_LIMITED_RANGE)
					*r2y_table = csc_table->r2y_bt601_12_235; /* bt601 limit */
				else
					*r2y_table = csc_table->r2y_bt601; /* bt601 full */
			}
		}
	} else {
		if (!is_input_yuv)
			return 0;

		/*
		 * is possible use bt2020 on rgb mode?
		 */
		if (WARN_ON(output_csc == DRM_COLOR_YCBCR_BT2020))
			return -EINVAL;

		if (input_csc == DRM_COLOR_YCBCR_BT2020)
			*y2r_table = csc_table->y2r_bt2020;
		else if (input_csc == DRM_COLOR_YCBCR_BT709)
			*y2r_table = csc_table->y2r_bt709;
		else if (input_csc == DRM_COLOR_YCBCR_BT601 &&
			 pstate->color_range == DRM_COLOR_YCBCR_LIMITED_RANGE)
			*y2r_table = csc_table->y2r_bt601_12_235; /* bt601 limit */
		else
			*y2r_table = csc_table->y2r_bt601;  /* bt601 full */

		if (input_csc == DRM_COLOR_YCBCR_BT2020)
			/*
			 * We don't have bt601 to bt709 table, force use bt709.
			 */
			*r2r_table = csc_table->r2r_bt2020_to_bt709;
	}

	return 0;
}

static int vop_csc_atomic_check(struct drm_crtc *crtc,
				struct drm_crtc_state *crtc_state)
{
	struct vop *vop = to_vop(crtc);
	struct drm_atomic_state *state = crtc_state->state;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	const struct vop_csc_table *csc_table = vop->data->csc_table;
	struct drm_plane_state *pstate;
	struct drm_plane *plane;
	bool is_input_yuv, is_output_yuv;
	int ret;

	is_output_yuv = is_yuv_output(s->bus_format);

	drm_atomic_crtc_state_for_each_plane(plane, crtc_state) {
		struct vop_plane_state *vop_plane_state;
		struct vop_win *win = to_vop_win(plane);

		pstate = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(pstate))
			return PTR_ERR(pstate);
		vop_plane_state = to_vop_plane_state(pstate);

		if (!pstate->fb)
			continue;
		is_input_yuv = is_yuv_support(pstate->fb->format->format);
		vop_plane_state->y2r_en = false;
		vop_plane_state->r2r_en = false;
		vop_plane_state->r2y_en = false;

		ret = vop_setup_csc_table(s, pstate, csc_table,
					  &vop_plane_state->y2r_table,
					  &vop_plane_state->r2r_table,
					  &vop_plane_state->r2y_table);
		if (ret)
			return ret;

		if (is_input_yuv && !is_output_yuv) {
			vop_plane_state->y2r_en = true;
			vop_plane_state->csc_mode = to_vop_csc_mode(pstate->color_encoding, pstate->color_range);
		} else if (!is_input_yuv && is_output_yuv) {
			vop_plane_state->r2y_en = true;
			vop_plane_state->csc_mode = to_vop_csc_mode(s->color_encoding, s->color_range);
		}

		if (csc_table) {
			vop_plane_state->y2r_en = !!vop_plane_state->y2r_table;
			vop_plane_state->r2r_en = !!vop_plane_state->r2r_table;
			vop_plane_state->r2y_en = !!vop_plane_state->r2y_table;
			continue;
		}

		/*
		 * This is update for IC design not reasonable, when enable
		 * hdr2sdr on rk3328, vop can't support per-pixel alpha * global
		 * alpha,so we must back to gpu, but gpu can't support hdr2sdr,
		 * gpu output hdr UI, vop will do:
		 * UI(rgbx) -> yuv -> rgb ->hdr2sdr -> overlay -> output.
		 */
		if (s->hdr.hdr2sdr_en &&
		    vop_plane_state->eotf == HDMI_EOTF_SMPTE_ST2084 &&
		    !is_yuv_support(pstate->fb->format->format))
			vop_plane_state->r2y_en = true;
		if (win->feature & WIN_FEATURE_PRE_OVERLAY)
			vop_plane_state->r2r_en =
				s->hdr.sdr2hdr_state.rgb2rgb_pre_conv_en;
		else if (win->feature & WIN_FEATURE_HDR2SDR)
			vop_plane_state->r2r_en =
				s->hdr.sdr2hdr_state.rgb2rgb_post_conv_en;
	}

	return 0;
}

static void vop_enable_debug_irq(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	uint32_t irqs;

	irqs = BUS_ERROR_INTR | WIN0_EMPTY_INTR | WIN1_EMPTY_INTR |
		WIN2_EMPTY_INTR | WIN3_EMPTY_INTR | HWC_EMPTY_INTR |
		POST_BUF_EMPTY_INTR;
	VOP_INTR_SET_TYPE(vop, clear, irqs, 1);
	VOP_INTR_SET_TYPE(vop, enable, irqs, 1);
}

static void vop_dsp_hold_valid_irq_enable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, clear, DSP_HOLD_VALID_INTR, 1);
	VOP_INTR_SET_TYPE(vop, enable, DSP_HOLD_VALID_INTR, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_dsp_hold_valid_irq_disable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, enable, DSP_HOLD_VALID_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

/*
 * (1) each frame starts at the start of the Vsync pulse which is signaled by
 *     the "FRAME_SYNC" interrupt.
 * (2) the active data region of each frame ends at dsp_vact_end
 * (3) we should program this same number (dsp_vact_end) into dsp_line_frag_num,
 *      to get "LINE_FLAG" interrupt at the end of the active on screen data.
 *
 * VOP_INTR_CTRL0.dsp_line_frag_num = VOP_DSP_VACT_ST_END.dsp_vact_end
 * Interrupts
 * LINE_FLAG -------------------------------+
 * FRAME_SYNC ----+                         |
 *                |                         |
 *                v                         v
 *                | Vsync | Vbp |  Vactive  | Vfp |
 *                        ^     ^           ^     ^
 *                        |     |           |     |
 *                        |     |           |     |
 * dsp_vs_end ------------+     |           |     |   VOP_DSP_VTOTAL_VS_END
 * dsp_vact_start --------------+           |     |   VOP_DSP_VACT_ST_END
 * dsp_vact_end ----------------------------+     |   VOP_DSP_VACT_ST_END
 * dsp_total -------------------------------------+   VOP_DSP_VTOTAL_VS_END
 */
static bool vop_line_flag_irq_is_enabled(struct vop *vop)
{
	uint32_t line_flag_irq;
	unsigned long flags;

	spin_lock_irqsave(&vop->irq_lock, flags);

	line_flag_irq = VOP_INTR_GET_TYPE(vop, enable, LINE_FLAG_INTR);

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	return !!line_flag_irq;
}

static void vop_line_flag_irq_enable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, clear, LINE_FLAG_INTR, 1);
	VOP_INTR_SET_TYPE(vop, enable, LINE_FLAG_INTR, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_line_flag_irq_disable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, enable, LINE_FLAG_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static int vop_core_clks_enable(struct vop *vop)
{
	int ret;

	ret = clk_enable(vop->hclk);
	if (ret < 0)
		return ret;

	ret = clk_enable(vop->aclk);
	if (ret < 0)
		goto err_disable_hclk;

	return 0;

err_disable_hclk:
	clk_disable(vop->hclk);
	return ret;
}

static void vop_core_clks_disable(struct vop *vop)
{
	clk_disable(vop->aclk);
	clk_disable(vop->hclk);
}

static void vop_wb_connector_reset(struct drm_connector *connector)
{
	struct vop_wb_connector_state *wb_state;

	if (connector->state) {
		__drm_atomic_helper_connector_destroy_state(connector->state);
		kfree(connector->state);
		connector->state = NULL;
	}

	wb_state = kzalloc(sizeof(*wb_state), GFP_KERNEL);
	if (wb_state)
		__drm_atomic_helper_connector_reset(connector, &wb_state->base);
}

static enum drm_connector_status
vop_wb_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void vop_wb_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static struct drm_connector_state *
vop_wb_connector_duplicate_state(struct drm_connector *connector)
{
	struct vop_wb_connector_state *wb_state;

	if (WARN_ON(!connector->state))
		return NULL;

	wb_state = kzalloc(sizeof(*wb_state), GFP_KERNEL);
	if (!wb_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector, &wb_state->base);

	return &wb_state->base;
}

static const struct drm_connector_funcs vop_wb_connector_funcs = {
	.reset = vop_wb_connector_reset,
	.detect = vop_wb_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = vop_wb_connector_destroy,
	.atomic_duplicate_state = vop_wb_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int vop_wb_connector_get_modes(struct drm_connector *connector)
{
	return 0;
}

static enum drm_mode_status
vop_wb_connector_mode_valid(struct drm_connector *connector,
			    struct drm_display_mode *mode)
{

	struct drm_writeback_connector *wb_conn;
	struct vop_wb *wb;
	struct vop *vop;
	int w, h;

	wb_conn = container_of(connector, struct drm_writeback_connector, base);
	wb = container_of(wb_conn, struct vop_wb, conn);
	vop = container_of(wb, struct vop, wb);
	w = mode->hdisplay;
	h = mode->vdisplay;

	if (w > vop->data->wb->max_output.width)
		return MODE_BAD_HVALUE;

	if (h > vop->data->wb->max_output.height)
		return MODE_BAD_VVALUE;

	return MODE_OK;
}

#define VOP_WB_BILI_SCL_FAC_CHECK(src, dst, fac)	\
				(fac * (dst - 1) >> 12 < (src - 1))

static uint16_t vop_wb_scale_down_factor(uint32_t src, uint32_t dst)
{
	uint32_t fac = 0;
	int i = 0;

	/*
	 * A workaround to avoid zero div.
	 */
	if (dst < 2)
		dst = 2;

	fac = scl_cal_scale2(src, dst);
	for (i = 0; i < 100; i++) {
		if (VOP_WB_BILI_SCL_FAC_CHECK(src, dst, fac))
			break;
		fac -= 1;
		DRM_DEBUG("down fac cali: src:%d, dst:%d, fac:0x%x\n", src, dst, fac);
	}

	return fac;
}

static inline bool
vop_wb_connector_changed_only(struct drm_crtc_state *cstate, struct drm_connector *conn)
{
	struct drm_crtc_state *old_state;
	u32 changed_connectors;

	old_state = drm_atomic_get_old_crtc_state(cstate->state, cstate->crtc);
	changed_connectors = cstate->connector_mask ^ old_state->connector_mask;

	return BIT(drm_connector_index(conn)) == changed_connectors;
}

static int vop_wb_encoder_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *cstate,
				struct drm_connector_state *conn_state)
{
	struct vop_wb_connector_state *wb_state = to_wb_state(conn_state);
	struct rockchip_crtc_state *vcstate = to_rockchip_crtc_state(cstate);
	struct drm_crtc *crtc = cstate->crtc;
	struct vop *vop = to_vop(crtc);
	struct drm_framebuffer *fb;
	struct drm_gem_object *obj, *uv_obj;
	struct rockchip_gem_object *rk_obj, *rk_uv_obj;
	uint32_t src_width;

	/*
	 * No need for a full modested when the only connector changed is the
	 * writeback connector.
	 */
	if (cstate->connectors_changed &&
	    vop_wb_connector_changed_only(cstate, conn_state->connector)) {
		cstate->connectors_changed = false;
		drm_dbg(crtc->dev,
			"VOP force change connectors_changed to false when only wb changed\n");
	}

	if (!conn_state->writeback_job || !conn_state->writeback_job->fb)
		return 0;

	fb = conn_state->writeback_job->fb;
	DRM_DEV_DEBUG(vop->dev, "%d x % d\n", fb->width, fb->height);

	if (!fb->format->is_yuv && is_yuv_output(vcstate->bus_format)) {
		DRM_ERROR("YUV2RGB is not supported by writeback\n");
		return -EINVAL;
	}

	if ((fb->width > cstate->mode.hdisplay) || (fb->width * 4 < cstate->mode.hdisplay) ||
	    ((fb->height < cstate->mode.vdisplay) &&
	    (fb->height != (cstate->mode.vdisplay >> 1)))) {
		drm_dbg_kms(crtc->dev, "Invalid framebuffer size %ux%u, Only support up to 1/4 x scale down and 1/2 y scale down\n",
			    fb->width, fb->height);
		return -EINVAL;
	}

	wb_state->xgt2_en = fb->width * 2 < cstate->mode.hdisplay;
	src_width = wb_state->xgt2_en ? cstate->mode.hdisplay / 2 : cstate->mode.hdisplay;
	wb_state->scale_x_factor = vop_wb_scale_down_factor(src_width, fb->width);
	wb_state->scale_x_en = (fb->width < cstate->mode.hdisplay) ? 1 : 0;
	wb_state->scale_y_en = (fb->height < cstate->mode.vdisplay) ? 1 : 0;

	wb_state->format = vop_convert_wb_format(fb->format->format);
	if (wb_state->format < 0) {
		drm_dbg_kms(crtc->dev, "Invalid pixel format %p4cc\n", &fb->format->format);
		return -EINVAL;
	}

	obj = fb->obj[0];
	rk_obj = to_rockchip_obj(obj);
	wb_state->yrgb_addr = rk_obj->dma_addr + fb->offsets[0];

	if (fb->format->is_yuv) {
		uv_obj = fb->obj[1];
		rk_uv_obj = to_rockchip_obj(uv_obj);

		wb_state->uv_addr = rk_uv_obj->dma_addr + fb->offsets[1];
	}

	return 0;
}

static void vop_wb_encoder_atomic_disable(struct drm_encoder *encoder,
					  struct drm_atomic_state *state)
{
	struct drm_crtc *crtc = encoder->crtc;

	if (!crtc->state->active_changed && !crtc->state->mode_changed) {
		crtc->state->connectors_changed = false;
		drm_dbg(crtc->dev,
			"VOP force change connectors_changed to false when disable wb\n");
	}
}

static const struct drm_encoder_helper_funcs vop_wb_encoder_helper_funcs = {
	.atomic_check = vop_wb_encoder_atomic_check,
	.atomic_disable = vop_wb_encoder_atomic_disable,
};

static const struct drm_connector_helper_funcs vop_wb_connector_helper_funcs = {
	.get_modes = vop_wb_connector_get_modes,
	.mode_valid = vop_wb_connector_mode_valid,
};

static int vop_wb_connector_init(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	struct drm_crtc *crtc = &vop->rockchip_crtc.crtc;
	int ret;

	if (!vop_data->wb)
		return 0;

	vop->wb.regs = vop_data->wb->regs;
	vop->wb.conn.encoder.possible_crtcs = drm_crtc_mask(crtc);
	drm_connector_helper_add(&vop->wb.conn.base, &vop_wb_connector_helper_funcs);

	ret = drm_writeback_connector_init(vop->drm_dev, &vop->wb.conn,
					   &vop_wb_connector_funcs,
					   &vop_wb_encoder_helper_funcs,
					   vop_data->wb->formats,
					   vop_data->wb->nformats,
					   vop->wb.conn.encoder.possible_crtcs);
	if (ret)
		DRM_DEV_ERROR(vop->dev, "writeback connector init failed\n");

	return ret;
}

static void vop_wb_connector_destory(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;

	if (!vop_data->wb)
		return;

	drm_encoder_cleanup(&vop->wb.conn.encoder);
	drm_connector_cleanup(&vop->wb.conn.base);
}

static void vop_wb_irqs_enable(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	const struct vop_intr *intr = vop_data->wb_intr;
	uint32_t irqs = VOPL_WB_UV_FIFO_FULL_INTR | VOPL_WB_YRGB_FIFO_FULL_INTR |
			VOPL_WB_COMPLETE_INTR;

	VOP_INTR_SET_TYPE2(vop, intr, enable, irqs, 1);
}

static uint32_t vop_read_and_clear_wb_irqs(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	const struct vop_intr *intr = vop_data->wb_intr;
	uint32_t irqs = VOPL_WB_UV_FIFO_FULL_INTR | VOPL_WB_YRGB_FIFO_FULL_INTR |
			VOPL_WB_COMPLETE_INTR;
	uint32_t val, ret;

	if (!intr)
		return 0;

	val = VOP_INTR_GET_TYPE2(vop, intr, status, irqs);
	ret = val;

	/* For RV1126B, it should use VOPL_WB_YRGB_FIFO_FULL_INTR to
	 * clear all wb interrupt.
	 */
	if (vop->version == VOP_VERSION_RV1126B)
		val |= VOPL_WB_YRGB_FIFO_FULL_INTR;

	if (val)
		VOP_INTR_SET_TYPE2(vop, intr, clear, val, 1);

	return ret;
}

static void vop_wb_commit(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *vcstate = to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);
	struct vop_wb *wb = &vop->wb;
	struct drm_writeback_connector *wb_conn = &wb->conn;
	struct drm_connector_state *conn_state = wb_conn->base.state;
	struct vop_wb_connector_state *wb_state;
	uint32_t fifo_throd;
	uint8_t r2y;

	if (!conn_state)
		return;
	wb_state = to_wb_state(conn_state);

	if (conn_state->writeback_job && conn_state->writeback_job->fb) {
		struct drm_framebuffer *fb = conn_state->writeback_job->fb;

		rockchip_drm_dbg(vop->dev, VOP_DEBUG_WB,
				 "Enable wb %ux%u fmt: %u pitches: %d addr: %pad",
				 fb->width, fb->height, wb_state->format,
				 fb->pitches[0], &wb_state->yrgb_addr);

		drm_writeback_queue_job(wb_conn, conn_state);
		if (!vop->enabled_win_mask) {
			drm_warn(vop->drm_dev, "Writeback can not work when all plane are disabled!");
			drm_writeback_signal_completion(&vop->wb.conn, 0);
			return;
		}

		fifo_throd = fb->pitches[0] >> 4;
		if (fifo_throd > vop->data->wb->fifo_depth)
			fifo_throd = vop->data->wb->fifo_depth;
		r2y = !vcstate->yuv_overlay && fb->format->is_yuv;

		VOP_CTRL_SET2(vop, wb, format, wb_state->format);
		VOP_CTRL_SET2(vop, wb, yrgb_mst, wb_state->yrgb_addr);
		VOP_CTRL_SET2(vop, wb, uv_mst, wb_state->uv_addr);
		VOP_CTRL_SET2(vop, wb, fifo_throd, fifo_throd);
		VOP_CTRL_SET2(vop, wb, xgt2_en, wb_state->xgt2_en);
		VOP_CTRL_SET2(vop, wb, scale_x_factor, wb_state->scale_x_factor);
		VOP_CTRL_SET2(vop, wb, scale_x_en, wb_state->scale_x_en);
		VOP_CTRL_SET2(vop, wb, scale_y_en, wb_state->scale_y_en);
		VOP_CTRL_SET2(vop, wb, r2y_en, r2y);

		VOP_CTRL_SET2(vop, wb, act_width, fb->width - 1);
		VOP_CTRL_SET2(vop, wb, vir_stride, fb->pitches[0] >> 2);
		VOP_CTRL_SET2(vop, wb, vir_stride_en, 1);
		VOP_CTRL_SET2(vop, wb, post_empty_stop_en, 1);

		/* RV1126B support one-shot mode */
		VOP_CTRL_SET2(vop, wb, one_frame_mode, 1);

		/* In one-shot mode, enable bit will auto clear */
		vop_write_reg_uncached(vop, &wb->regs->enable, 1);

		vop_wb_irqs_enable(vop);
	}
}

static void __maybe_unused vop_wb_disable(struct vop *vop)
{
	struct vop_wb *wb = &vop->wb;

	VOP_CTRL_SET2(vop, wb, enable, 0);
	vop_wb_cfg_done(vop);
}

static void vop_crtc_load_lut(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	int i, dle, lut_idx = 0;

	if (!vop->is_enabled || !vop->lut || !vop->lut_regs)
		return;

	if (!VOP_CTRL_SUPPORT(vop, update_gamma_lut)) {
		spin_lock(&vop->reg_lock);
		VOP_CTRL_SET(vop, dsp_lut_en, 0);
		vop_cfg_done(vop);
		spin_unlock(&vop->reg_lock);

#define CTRL_GET(name) VOP_CTRL_GET(vop, name)
		readx_poll_timeout(CTRL_GET, dsp_lut_en,
				dle, !dle, 5, 33333);
	} else {
		lut_idx = CTRL_GET(lut_buffer_index);
	}

	for (i = 0; i < vop->lut_len; i++)
		vop_write_lut(vop, i << 2, vop->lut[i]);

	spin_lock(&vop->reg_lock);

	VOP_CTRL_SET(vop, dsp_lut_en, 1);
	VOP_CTRL_SET(vop, update_gamma_lut, 1);
	vop_cfg_done(vop);
	vop->lut_active = true;

	spin_unlock(&vop->reg_lock);

	if (VOP_CTRL_SUPPORT(vop, update_gamma_lut)) {
		readx_poll_timeout(CTRL_GET, lut_buffer_index,
				   dle, dle != lut_idx, 5, 33333);
		/* FIXME:
		 * update_gamma value auto clean to 0 by HW, should not
		 * bakeup it.
		 */
		VOP_CTRL_SET(vop, update_gamma_lut, 0);
	}
#undef CTRL_GET
}

static void rockchip_vop_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red,
					   u16 green, u16 blue, int regno)
{
	struct vop *vop = to_vop(crtc);
	u32 lut_len = vop->lut_len;
	u32 r, g, b;

	if (regno >= lut_len || !vop->lut)
		return;

	r = red * (lut_len - 1) / 0xffff;
	g = green * (lut_len - 1) / 0xffff;
	b = blue * (lut_len - 1) / 0xffff;
	vop->lut[regno] = r * lut_len * lut_len + g * lut_len + b;
}

static void rockchip_vop_crtc_fb_gamma_get(struct drm_crtc *crtc, u16 *red,
					   u16 *green, u16 *blue, int regno)
{
	struct vop *vop = to_vop(crtc);
	u32 lut_len = vop->lut_len;
	u32 r, g, b;

	if (regno >= lut_len || !vop->lut)
		return;

	r = (vop->lut[regno] / lut_len / lut_len) & (lut_len - 1);
	g = (vop->lut[regno] / lut_len) & (lut_len - 1);
	b = vop->lut[regno] & (lut_len - 1);
	*red = r * 0xffff / (lut_len - 1);
	*green = g * 0xffff / (lut_len - 1);
	*blue = b * 0xffff / (lut_len - 1);
}

static int vop_crtc_legacy_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green,
				     u16 *blue, uint32_t size,
				     struct drm_modeset_acquire_ctx *ctx)
{
	struct vop *vop = to_vop(crtc);
	int len = min(size, vop->lut_len);
	int i;

	if (!vop->lut)
		return -EINVAL;

	for (i = 0; i < len; i++)
		rockchip_vop_crtc_fb_gamma_set(crtc, red[i], green[i], blue[i], i);

	vop_crtc_load_lut(crtc);

	return 0;
}

static int vop_crtc_atomic_gamma_set(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_state)
{
	struct vop *vop = to_vop(crtc);
	struct drm_color_lut *lut = vop->gamma_lut;
	unsigned int i;

	for (i = 0; i < vop->lut_len; i++)
		rockchip_vop_crtc_fb_gamma_set(crtc, lut[i].red, lut[i].green,
					       lut[i].blue, i);
	vop_crtc_load_lut(crtc);

	return 0;
}

static void vop_regsbak(struct vop *vop)
{
	int i;

	/*
	 * No need to backup DSC/GAMMA_LUT/BPP_LUT/MMU
	 */
	for (i = 0; i < vop->len; i += sizeof(u32))
		vop->regsbak[i / 4] = readl_relaxed(vop->regs + i);
}

static void vop_power_enable(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	int ret;

	ret = clk_prepare_enable(vop->hclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to enable hclk - %d\n", ret);
		return;
	}

	ret = clk_prepare_enable(vop->dclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to enable dclk - %d\n", ret);
		goto err_disable_hclk;
	}

	ret = clk_prepare_enable(vop->aclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to enable aclk - %d\n", ret);
		goto err_disable_dclk;
	}

	if (vop->genpd_dev0) {
		ret = pm_runtime_resume_and_get(vop->genpd_dev0);
		if (ret < 0) {
			dev_err(vop->dev,
				"failed to get pm runtime for pd0, ret = %d\n", ret);
			goto err_disable_aclk;
		}
	}

	if (vop->genpd_dev1) {
		ret = pm_runtime_resume_and_get(vop->genpd_dev1);
		if (ret < 0) {
			dev_err(vop->dev,
				"failed to get pm runtime for pd1, ret = %d\n", ret);
			goto err_put_genpd_dev0;
		}
	}

	ret = pm_runtime_resume_and_get(vop->dev);
	if (ret < 0) {
		dev_err(vop->dev, "failed to get pm runtime: %d\n", ret);
		goto err_put_genpd_dev1;
	}

	vop_regsbak(vop);

	if (VOP_CTRL_SUPPORT(vop, version)) {
		uint32_t version = VOP_CTRL_GET(vop, version);

		/*
		 * Fixup rk3288w version.
		 */
		if (version && version == 0x0a05)
			vop->version = VOP_VERSION(3, 1);
	}

	vop->is_enabled = true;

	return;

err_put_genpd_dev1:
	pm_runtime_put_sync(vop->genpd_dev1);
err_put_genpd_dev0:
	pm_runtime_put_sync(vop->genpd_dev0);
err_disable_aclk:
	clk_disable_unprepare(vop->aclk);
err_disable_dclk:
	clk_disable_unprepare(vop->dclk);
err_disable_hclk:
	clk_disable_unprepare(vop->hclk);
}

static void vop_initial(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;
	struct vop_wb *wb = &vop->wb;
	int i;

	vop_power_enable(crtc);

	VOP_CTRL_SET(vop, global_regdone_en, 1);
	VOP_CTRL_SET(vop, dsp_blank, 0);
	VOP_CTRL_SET(vop, axi_outstanding_max_num, 30);
	VOP_CTRL_SET(vop, axi_max_outstanding_en, 1);
	VOP_CTRL_SET(vop, dither_up_en, 1);

	/*
	 * We need to make sure that all windows are disabled before resume
	 * the crtc. Otherwise we might try to scan from a destroyed
	 * buffer later.
	 */
	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *win = &vop->win[i];
		int channel = i * 2 + 1;

		VOP_WIN_SET(vop, win, channel, (channel + 1) << 4 | channel);
	}
	VOP_CTRL_SET(vop, afbdc_en, 0);
	vop_enable_debug_irq(crtc);

	if (vop->version == VOP_VERSION_RK3576_LITE) {
		VOP_GRF_SET(vop, grf, grf_vopl_sel, 1);
		VOP_CTRL_SET(vop, enable, 1);
	}

	if (vop_data->wb) {
		VOP_CTRL_SET2(vop, wb, axi_yrgb_id, vop_data->wb->axi_yrgb_id);
		VOP_CTRL_SET2(vop, wb, axi_uv_id, vop_data->wb->axi_uv_id);
		vop_wb_cfg_done(vop);
	}
}

static void vop_crtc_atomic_disable_for_psr(struct drm_crtc *crtc,
					    struct drm_crtc_state *old_state)
{
	struct vop *vop = to_vop(crtc);

	/*
	 * For mcu interface, if mcu_hold_mode is enabled, the wins will stop
	 * accessing DDR and the interface will also stop output.
	 *
	 * In addition, the regs operations in vop_disable_all_planes() will
	 * not take effect when the mcu_hold_mode is enabled.
	 */
	if (vop->mcu_timing.mcu_pix_total)
		VOP_CTRL_SET(vop, mcu_hold_mode, 1);
	else
		vop_disable_all_planes(vop);
	vop->aclk_rate = clk_get_rate(vop->aclk);
	clk_set_rate(vop->aclk, vop->aclk_rate / 3);
	vop->aclk_rate_reset = true;
}

static void vop_crtc_atomic_disable(struct drm_crtc *crtc,
				    struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_state = drm_atomic_get_old_crtc_state(state, crtc);
	struct vop *vop = to_vop(crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	int sys_status = drm_crtc_index(crtc) ?
				SYS_STATUS_LCDC1 : SYS_STATUS_LCDC0;
	unsigned long status;

	WARN_ON(vop->event);

	if (crtc->state->self_refresh_active) {
		vop_crtc_atomic_disable_for_psr(crtc, old_state);
		goto out;
	}

	vop_lock(vop);
	if (s->hold_mode) {
		VOP_CTRL_SET(vop, edpi_te_en, 0);
		VOP_CTRL_SET(vop, edpi_ctrl_mode, 0);
	}
	VOP_CTRL_SET(vop, reg_done_frm, 1);
	VOP_CTRL_SET(vop, dsp_interlace, 0);
	drm_crtc_vblank_off(crtc);
	VOP_CTRL_SET(vop, out_mode, ROCKCHIP_OUT_MODE_P888);
	VOP_CTRL_SET(vop, afbdc_en, 0);
	VOP_CTRL_SET(vop, out_dresetn, 0);
	vop_disable_all_planes(vop);

	/*
	 * Vop standby will take effect at end of current frame,
	 * if dsp hold valid irq happen, it means standby complete.
	 *
	 * we must wait standby complete when we want to disable aclk,
	 * if not, memory bus maybe dead.
	 */
	reinit_completion(&vop->dsp_hold_completion);
	vop_dsp_hold_valid_irq_enable(vop);

	spin_lock(&vop->reg_lock);

	VOP_CTRL_SET(vop, standby, 1);

	spin_unlock(&vop->reg_lock);

	if (!wait_for_completion_timeout(&vop->dsp_hold_completion,
					 msecs_to_jiffies(200)))
		WARN(1, "%s: timed out waiting for DSP hold", crtc->name);

	vop_dsp_hold_valid_irq_disable(vop);

	vop->is_enabled = false;
	if (vop->is_iommu_enabled) {
		/*
		 * vop standby complete, so iommu detach is safe.
		 */
		VOP_CTRL_SET(vop, dma_stop, 1);
		rockchip_drm_dma_detach_device(vop->drm_dev, vop->dev);
		vop->is_iommu_enabled = false;
	}

	pm_runtime_put_sync(vop->dev);

	if (vop->genpd_dev1)
		pm_runtime_put_sync(vop->genpd_dev1);

	if (vop->genpd_dev0)
		pm_runtime_put_sync(vop->genpd_dev0);

	clk_disable_unprepare(vop->dclk);
	clk_disable_unprepare(vop->aclk);
	clk_disable_unprepare(vop->hclk);
	vop_unlock(vop);

	rockchip_clear_system_status(sys_status);
	status = rockchip_get_system_status();
	if (!(status & SYS_STATUS_LCDC0) && !(status & SYS_STATUS_LCDC1))
		rockchip_request_early_suspend();

out:
	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}
}

static int vop_plane_prepare_fb(struct drm_plane *plane,
				struct drm_plane_state *new_state)
{
	if (plane->state->fb)
		drm_framebuffer_get(plane->state->fb);

	return 0;
}

static void vop_plane_cleanup_fb(struct drm_plane *plane,
				 struct drm_plane_state *old_state)
{
	if (old_state->fb)
		drm_framebuffer_put(old_state->fb);
}

static bool rockchip_vop_mod_supported(struct drm_plane *plane,
				       u32 format, u64 modifier)
{
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	if (!rockchip_afbc(plane, modifier)) {
		DRM_DEBUG_KMS("Unsupported format modifier 0x%llx\n", modifier);

		return false;
	}

	return vop_convert_afbc_format(format) >= 0;
}

static int vop_plane_atomic_check(struct drm_plane *plane,
			   struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb = new_plane_state->fb;
	struct vop_win *win = to_vop_win(plane);
	struct vop_plane_state *vop_plane_state = to_vop_plane_state(new_plane_state);
	const struct vop_data *vop_data;
	struct vop *vop;
	int ret;
	struct drm_rect *dest = &vop_plane_state->dest;
	struct drm_rect *src = &vop_plane_state->src;
	struct drm_gem_object *obj, *uv_obj;
	struct rockchip_gem_object *rk_obj, *rk_uv_obj;
	int min_scale = win->phy->scl ? FRAC_16_16(1, 8) :
					DRM_PLANE_NO_SCALING;
	int max_scale = win->phy->scl ? FRAC_16_16(8, 1) :
					DRM_PLANE_NO_SCALING;
	unsigned long offset;
	dma_addr_t dma_addr;

	crtc = crtc ? crtc : plane->state->crtc;
	if (!crtc || !fb) {
		plane->state->visible = false;
		return 0;
	}

	crtc_state = drm_atomic_get_existing_crtc_state(state,
							crtc);
	if (WARN_ON(!crtc_state))
		return -EINVAL;

	vop_plane_state->zpos = new_plane_state->zpos;
	vop_plane_state->global_alpha = new_plane_state->alpha >> 8;
	vop_plane_state->blend_mode = new_plane_state->pixel_blend_mode;

	ret = drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
						  min_scale, max_scale,
						  true, true);
	if (ret)
		return ret;

	if (!new_plane_state->visible) {
		DRM_ERROR("%s is invisible(src: pos[%d, %d] rect[%d x %d] dst: pos[%d, %d] rect[%d x %d]\n",
			  plane->name, new_plane_state->src_x >> 16, new_plane_state->src_y >> 16, new_plane_state->src_w >> 16,
			  new_plane_state->src_h >> 16, new_plane_state->crtc_x, new_plane_state->crtc_y, new_plane_state->crtc_w,
			  new_plane_state->crtc_h);
		return 0;
	}

	src->x1 = new_plane_state->src.x1;
	src->y1 = new_plane_state->src.y1;
	src->x2 = new_plane_state->src.x2;
	src->y2 = new_plane_state->src.y2;
	dest->x1 = new_plane_state->dst.x1;
	dest->y1 = new_plane_state->dst.y1;
	dest->x2 = new_plane_state->dst.x2;
	dest->y2 = new_plane_state->dst.y2;

	vop_plane_state->format = vop_convert_format(fb->format->format);
	if (vop_plane_state->format < 0)
		return vop_plane_state->format;

	vop = to_vop(crtc);
	vop_data = vop->data;

	if (VOP_MAJOR(vop->version) == 2 && is_alpha_support(fb->format->format) &&
	    vop_plane_state->global_alpha != 0xff) {
		DRM_ERROR("Pixel alpha and global alpha can't be enabled at the same time\n");
		return -EINVAL;
	}

	if (drm_rect_width(src) >> 16 < 4 || drm_rect_height(src) >> 16 < 4 ||
	    drm_rect_width(dest) < 4 || drm_rect_width(dest) < 4) {
		DRM_ERROR("Invalid size: %dx%d->%dx%d, min size is 4x4\n",
			  drm_rect_width(src) >> 16, drm_rect_height(src) >> 16,
			  drm_rect_width(dest), drm_rect_height(dest));
		new_plane_state->visible = false;
		return 0;
	}

	if (drm_rect_width(src) >> 16 > vop_data->max_input.width ||
	    drm_rect_height(src) >> 16 > vop_data->max_input.height) {
		DRM_ERROR("Invalid source: %dx%d. max input: %dx%d\n",
			  drm_rect_width(src) >> 16,
			  drm_rect_height(src) >> 16,
			  vop_data->max_input.width,
			  vop_data->max_input.height);
		return -EINVAL;
	}

	/*
	 * Src.x1 can be odd when do clip, but yuv plane start point
	 * need align with 2 pixel.
	 */
	if (fb->format->is_yuv && ((new_plane_state->src.x1 >> 16) % 2)) {
		DRM_DEBUG_KMS("Invalid Source: Yuv format not support odd xpos\n");
		return -EINVAL;
	}

	if (fb->format->is_yuv && new_plane_state->rotation & DRM_MODE_REFLECT_Y) {
		DRM_DEBUG_KMS("Invalid Source: Yuv format does not support this rotation\n");
		return -EINVAL;
	}

	if (rockchip_afbc(plane, new_plane_state->fb->modifier)) {
		if (new_plane_state->src.x1 || new_plane_state->src.y1) {
			DRM_DEBUG_KMS("AFBC does not support offset display, " \
				      "xpos=%d, ypos=%d, offset=%d\n",
				      new_plane_state->src.x1, new_plane_state->src.y1,
				      fb->offsets[0]);
			return -EINVAL;
		}

		if (new_plane_state->rotation && new_plane_state->rotation != DRM_MODE_ROTATE_0) {
			DRM_DEBUG_KMS("No rotation support in AFBC, rotation=%d\n",
				      new_plane_state->rotation);
			return -EINVAL;
		}

		/* RK3399 AFBC act_h must aligned as 16 line */
		if (vop->version == VOP_VERSION_RK3399_BIG) {
			u32 old_act_h = drm_rect_height(src) >> 16;

			if (!IS_ALIGNED(old_act_h, 16)) {
				u32 aligned_act_h = ALIGN_DOWN(old_act_h, 16);

				src->y2 = ((src->y1 >> 16) + aligned_act_h) << 16;

				DRM_DEBUG_KMS("Win%d afbc act_h:%d isn't aligned as 16, so change to: %d\n",
					      win->win_id, old_act_h, aligned_act_h);
			}
		}
	}

	offset = (src->x1 >> 16) * fb->format->cpp[0];
	vop_plane_state->offset = offset + fb->offsets[0];
	if (new_plane_state->rotation & DRM_MODE_REFLECT_Y)
		offset += ((src->y2 >> 16) - 1) * fb->pitches[0];
	else
		offset += (src->y1 >> 16) * fb->pitches[0];

	obj = fb->obj[0];
	rk_obj = to_rockchip_obj(obj);
	vop_plane_state->yrgb_mst = rk_obj->dma_addr + offset + fb->offsets[0];
	if (fb->format->is_yuv) {
		int hsub = fb->format->hsub;
		int vsub = fb->format->vsub;

		offset = (src->x1 >> 16) * fb->format->cpp[1] / hsub;
		offset += (src->y1 >> 16) * fb->pitches[1] / vsub;

		uv_obj = fb->obj[1];
		rk_uv_obj = to_rockchip_obj(uv_obj);

		dma_addr = rk_uv_obj->dma_addr + offset + fb->offsets[1];
		vop_plane_state->uv_mst = dma_addr;
	}

	return 0;
}

static void vop_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									   plane);
	struct vop_win *win = to_vop_win(plane);
	struct vop *vop = to_vop(old_state->crtc);

	if (!old_state->crtc)
		return;

	rockchip_drm_dbg_thread_info(vop->dev, VOP_DEBUG_PLANE, "disable win%d-area%d",
				     win->win_id, win->area_id);

	spin_lock(&vop->reg_lock);

	vop_win_disable(vop, win);

	/*
	 * IC design bug: in the bandwidth tension environment when close win2,
	 * vop will access the freed memory lead to iommu pagefault.
	 * so we add this reset to workaround.
	 */
	if (vop->version == VOP_VERSION_PX30_LITE && win->win_id == 2)
		VOP_WIN_SET(vop, win, yrgb_mst, 0);

	spin_unlock(&vop->reg_lock);
}

static void vop_plane_setup_color_key(struct drm_plane *plane)
{
	struct drm_plane_state *pstate = plane->state;
	struct vop_plane_state *vpstate = to_vop_plane_state(pstate);
	struct drm_framebuffer *fb = pstate->fb;
	struct vop_win *win = to_vop_win(plane);
	struct vop *vop = win->vop;
	uint32_t color_key_en = 0;
	uint32_t color_key;
	uint32_t r = 0;
	uint32_t g = 0;
	uint32_t b = 0;

	if (!(vpstate->color_key & VOP_COLOR_KEY_MASK) || fb->format->is_yuv) {
		VOP_WIN_SET(vop, win, color_key_en, 0);
		return;
	}

	switch (fb->format->format) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		r = (vpstate->color_key & 0xf800) >> 11;
		g = (vpstate->color_key & 0x7e0) >> 5;
		b = (vpstate->color_key & 0x1f);
		if (VOP_WIN_SUPPORT(vop, win, fmt_10)) {
			r <<= 5;
			g <<= 4;
			b <<= 5;
		} else {
			r <<= 3;
			g <<= 2;
			b <<= 3;
		}
		color_key_en = 1;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		r = (vpstate->color_key & 0xff0000) >> 16;
		g = (vpstate->color_key & 0xff00) >> 8;
		b = (vpstate->color_key & 0xff);
		if (VOP_WIN_SUPPORT(vop, win, fmt_10)) {
			r <<= 2;
			g <<= 2;
			b <<= 2;
		}
		color_key_en = 1;
		break;
	}

	if (VOP_WIN_SUPPORT(vop, win, fmt_10))
		color_key = (r << 20) | (g << 10) | b;
	else
		color_key = (r << 16) | (g << 8) | b;

	VOP_WIN_SET(vop, win, color_key_en, color_key_en);
	VOP_WIN_SET(vop, win, color_key, color_key);
}

static void vop_plane_atomic_update(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct drm_crtc *crtc = new_state->crtc;
	struct drm_display_mode *mode = NULL;
	struct vop_win *win = to_vop_win(plane);
	struct vop_plane_state *vop_plane_state = to_vop_plane_state(new_state);
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct rockchip_crtc_state *s;
	struct vop *vop = to_vop(new_state->crtc);
	struct drm_framebuffer *fb = new_state->fb;
	unsigned int actual_w, actual_h, dsp_w, dsp_h;
	unsigned int dsp_stx, dsp_sty;
	uint32_t act_info, dsp_info, dsp_st;
	struct drm_rect *src = &vop_plane_state->src;
	struct drm_rect *dest = &vop_plane_state->dest;
	const uint32_t *y2r_table = vop_plane_state->y2r_table;
	const uint32_t *r2r_table = vop_plane_state->r2r_table;
	const uint32_t *r2y_table = vop_plane_state->r2y_table;
	uint32_t val;
	bool rb_swap, global_alpha_en, uv_swap;
	int is_yuv = fb->format->is_yuv;
	bool afbc_en = false;

	/*
	 * can't update plane when vop is disabled.
	 */
	if (WARN_ON(!crtc))
		return;

	if (WARN_ON(!vop->is_enabled))
		return;

	if (!new_state->visible) {
		vop_plane_atomic_disable(plane, state);
		return;
	}

	mode = &crtc->state->adjusted_mode;
	actual_w = drm_rect_width(src) >> 16;
	actual_h = drm_rect_height(src) >> 16;

	dsp_w = drm_rect_width(dest);
	if (dest->x1 + dsp_w > adjusted_mode->hdisplay) {
		DRM_ERROR("%s win%d dest->x1[%d] + dsp_w[%d] exceed mode hdisplay[%d]\n",
			  crtc->name, win->win_id, dest->x1, dsp_w, adjusted_mode->hdisplay);
		dsp_w = adjusted_mode->hdisplay - dest->x1;
		if (dsp_w < 4)
			dsp_w = 4;
		actual_w = dsp_w * actual_w / drm_rect_width(dest);
	}
	dsp_h = drm_rect_height(dest);
	if (dest->y1 + dsp_h > adjusted_mode->vdisplay) {
		DRM_ERROR("%s win%d dest->y1[%d] + dsp_h[%d] exceed mode vdisplay[%d]\n",
			  crtc->name, win->win_id, dest->y1, dsp_h, adjusted_mode->vdisplay);
		dsp_h = adjusted_mode->vdisplay - dest->y1;
		if (dsp_h < 4)
			dsp_h = 4;
		actual_h = dsp_h * actual_h / drm_rect_height(dest);
	}
	if ((vop->version == VOP_VERSION_RK3036 ||
	     vop->version == VOP_VERSION_RK3506 ||
	     vop->version == VOP_VERSION_RK3576_LITE ||
	     vop->version == VOP_VERSION_RV1126B) &&
	    (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE))
		dsp_h = dsp_h / 2;

	act_info = (actual_h - 1) << 16 | ((actual_w - 1) & 0xffff);

	/*
	 * For y-mirroring we need to move address
	 * to the beginning of the last line.
	 */
	dsp_info = (dsp_h - 1) << 16;
	dsp_info |= (dsp_w - 1) & 0xffff;

	dsp_stx = dest->x1 + mode->crtc_htotal - mode->crtc_hsync_start;
	dsp_sty = dest->y1 + mode->crtc_vtotal - mode->crtc_vsync_start;
	if ((vop->version == VOP_VERSION_RK3036 ||
	     vop->version == VOP_VERSION_RK3506 ||
	     vop->version == VOP_VERSION_RK3576_LITE ||
	     vop->version == VOP_VERSION_RV1126B) &&
	    (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE))
		dsp_sty = dest->y1 / 2 + mode->crtc_vtotal - mode->crtc_vsync_start;
	dsp_st = dsp_sty << 16 | (dsp_stx & 0xffff);

	s = to_rockchip_crtc_state(crtc->state);
	spin_lock(&vop->reg_lock);

	VOP_WIN_SET(vop, win, format, vop_plane_state->format);

	VOP_WIN_SET(vop, win, interlace_read,
		    (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) ? 1 : 0);

	VOP_WIN_SET(vop, win, yrgb_vir, DIV_ROUND_UP(fb->pitches[0], 4));
	VOP_WIN_SET(vop, win, yrgb_mst, vop_plane_state->yrgb_mst);

	VOP_WIN_SET(vop, win, ymirror,
		    (new_state->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0);
	VOP_WIN_SET(vop, win, xmirror,
		    (new_state->rotation & DRM_MODE_REFLECT_X) ? 1 : 0);

	if (is_yuv) {
		VOP_WIN_SET(vop, win, uv_vir, DIV_ROUND_UP(fb->pitches[1], 4));
		VOP_WIN_SET(vop, win, uv_mst, vop_plane_state->uv_mst);
		uv_swap = has_uv_swapped(fb->format->format);
		VOP_WIN_SET(vop, win, uv_swap, uv_swap);
	}
	VOP_WIN_SET(vop, win, fmt_10, is_yuv_10bit(fb->format->format));
	VOP_WIN_SET(vop, win, fmt_yuyv, is_yuyv_format(fb->format->format));

	if (win->phy->scl)
		scl_vop_cal_scl_fac(vop, win, actual_w, actual_h,
				    drm_rect_width(dest), dsp_h,
				    fb->format->format);

	if (VOP_WIN_SUPPORT(vop, win, color_key))
		vop_plane_setup_color_key(&win->base);

	VOP_WIN_SET(vop, win, act_info, act_info);
	VOP_WIN_SET(vop, win, dsp_info, dsp_info);
	VOP_WIN_SET(vop, win, dsp_st, dsp_st);

	rb_swap = has_rb_swapped(vop->data->version, fb->format->format);
	VOP_WIN_SET(vop, win, rb_swap, rb_swap);

	global_alpha_en = (vop_plane_state->global_alpha == 0xff) ? 0 : 1;
	if ((is_alpha_support(fb->format->format) || global_alpha_en) &&
	    ((vop->version != VOP_VERSION_RV1126B && (s->dsp_layer_sel & 0x3) != win->win_id) ||
	     (vop->version == VOP_VERSION_RV1126B && (s->dsp_layer_sel & BIT(win->win_id))))) {
		int src_blend_m0;
		int pre_multi_alpha = ALPHA_SRC_PRE_MUL;

		if (is_alpha_support(fb->format->format) && global_alpha_en)
			src_blend_m0 = ALPHA_PER_PIX_GLOBAL;
		else if (is_alpha_support(fb->format->format))
			src_blend_m0 = ALPHA_PER_PIX;
		else
			src_blend_m0 = ALPHA_GLOBAL;

		if (vop_plane_state->blend_mode != DRM_MODE_BLEND_PREMULTI || src_blend_m0 == ALPHA_GLOBAL)
			pre_multi_alpha = ALPHA_SRC_NO_PRE_MUL;

		VOP_WIN_SET(vop, win, dst_alpha_ctl,
			    DST_FACTOR_M0(ALPHA_SRC_INVERSE));
		val = SRC_ALPHA_EN(1) | SRC_COLOR_M0(pre_multi_alpha) |
			SRC_ALPHA_M0(ALPHA_STRAIGHT) |
			SRC_BLEND_M0(src_blend_m0) |
			SRC_ALPHA_CAL_M0(ALPHA_SATURATION) |
			SRC_FACTOR_M0(global_alpha_en ?
				      ALPHA_SRC_GLOBAL : ALPHA_ONE);
		VOP_WIN_SET(vop, win, src_alpha_ctl, val);
		VOP_WIN_SET(vop, win, alpha_pre_mul, !pre_multi_alpha); /* VOP lite only */
		VOP_WIN_SET(vop, win, alpha_mode, src_blend_m0); /* VOP lite only */
		VOP_WIN_SET(vop, win, alpha_en, 1);
	} else {
		VOP_WIN_SET(vop, win, src_alpha_ctl, SRC_ALPHA_EN(0));
		VOP_WIN_SET(vop, win, alpha_en, 0);
	}
	VOP_WIN_SET(vop, win, global_alpha_val, vop_plane_state->global_alpha);

	VOP_WIN_SET(vop, win, csc_mode, vop_plane_state->csc_mode);
	if (win->csc) {
		vop_load_csc_table(vop, win->csc->y2r_offset, y2r_table);
		vop_load_csc_table(vop, win->csc->r2r_offset, r2r_table);
		vop_load_csc_table(vop, win->csc->r2y_offset, r2y_table);
		VOP_WIN_SET_EXT(vop, win, csc, y2r_en, vop_plane_state->y2r_en);
		VOP_WIN_SET_EXT(vop, win, csc, r2r_en, vop_plane_state->r2r_en);
		VOP_WIN_SET_EXT(vop, win, csc, r2y_en, vop_plane_state->r2y_en);
		VOP_WIN_SET_EXT(vop, win, csc, csc_mode, vop_plane_state->csc_mode);
	}
	VOP_WIN_SET(vop, win, enable, 1);
	VOP_WIN_SET(vop, win, gate, 1);
	spin_unlock(&vop->reg_lock);

	vop->enabled_win_mask |= BIT(win->win_id);

	if (rockchip_afbc(plane, fb->modifier))
		afbc_en = true;
	rockchip_drm_dbg_thread_info(vop->dev, VOP_DEBUG_PLANE,
				     "update win%d-area%d [%dx%d->%dx%d@(%d, %d)] zpos:%d fmt[%p4cc%s] addr[%pad]",
				     win->win_id, win->area_id, actual_w, actual_h,
				     dsp_w, dsp_h, dest->x1, dest->y1, vop_plane_state->zpos, &fb->format->format,
				     afbc_en ? "[AFBC]" : "",
				     &vop_plane_state->yrgb_mst);
}

static int vop_plane_atomic_async_check(struct drm_plane *plane,
					struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct vop_win *win = to_vop_win(plane);
	int min_scale = win->phy->scl ? FRAC_16_16(1, 8) :
					DRM_PLANE_NO_SCALING;
	int max_scale = win->phy->scl ? FRAC_16_16(8, 1) :
					DRM_PLANE_NO_SCALING;
	struct drm_crtc_state *crtc_state;

	if (plane != new_plane_state->crtc->cursor)
		return -EINVAL;

	if (!plane->state)
		return -EINVAL;

	if (!plane->state->fb)
		return -EINVAL;

	crtc_state = drm_atomic_get_existing_crtc_state(state, new_plane_state->crtc);

	/* Special case for asynchronous cursor updates. */
	if (!crtc_state)
		crtc_state = plane->crtc->state;

	return drm_atomic_helper_check_plane_state(plane->state, crtc_state,
						   min_scale, max_scale,
						   true, true);
}

static void vop_plane_atomic_async_update(struct drm_plane *plane,
					  struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct vop *vop = to_vop(plane->state->crtc);
	struct drm_framebuffer *old_fb = plane->state->fb;

	plane->state->crtc_x = new_state->crtc_x;
	plane->state->crtc_y = new_state->crtc_y;
	plane->state->crtc_h = new_state->crtc_h;
	plane->state->crtc_w = new_state->crtc_w;
	plane->state->src_x = new_state->src_x;
	plane->state->src_y = new_state->src_y;
	plane->state->src_h = new_state->src_h;
	plane->state->src_w = new_state->src_w;
	swap(plane->state->fb, new_state->fb);

	if (vop->is_enabled) {
		vop_plane_atomic_update(plane, state);
		spin_lock(&vop->reg_lock);
		vop_cfg_done(vop);
		spin_unlock(&vop->reg_lock);

		/*
		 * A scanout can still be occurring, so we can't drop the
		 * reference to the old framebuffer. To solve this we get a
		 * reference to old_fb and set a worker to release it later.
		 * FIXME: if we perform 500 async_update calls before the
		 * vblank, then we can have 500 different framebuffers waiting
		 * to be released.
		 */
		if (old_fb && plane->state->fb != old_fb) {
			drm_framebuffer_get(old_fb);
			WARN_ON(drm_crtc_vblank_get(plane->state->crtc) != 0);
			drm_flip_work_queue(&vop->fb_unref_work, old_fb);
			set_bit(VOP_PENDING_FB_UNREF, &vop->pending);
		}
	}
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.prepare_fb = vop_plane_prepare_fb,
	.cleanup_fb = vop_plane_cleanup_fb,
	.atomic_check = vop_plane_atomic_check,
	.atomic_update = vop_plane_atomic_update,
	.atomic_disable = vop_plane_atomic_disable,
	.atomic_async_check = vop_plane_atomic_async_check,
	.atomic_async_update = vop_plane_atomic_async_update,
};

/**
 * rockchip_atomic_helper_update_plane - copy from drm_atomic_helper_update_plane
 * be designed to support async commit at ioctl DRM_IOCTL_MODE_SETPLANE.
 * @plane: plane object to update
 * @crtc: owning CRTC of owning plane
 * @fb: framebuffer to flip onto plane
 * @crtc_x: x offset of primary plane on crtc
 * @crtc_y: y offset of primary plane on crtc
 * @crtc_w: width of primary plane rectangle on crtc
 * @crtc_h: height of primary plane rectangle on crtc
 * @src_x: x offset of @fb for panning
 * @src_y: y offset of @fb for panning
 * @src_w: width of source rectangle in @fb
 * @src_h: height of source rectangle in @fb
 * @ctx: lock acquire context
 *
 * Provides a default plane update handler using the atomic driver interface.
 *
 * RETURNS:
 * Zero on success, error code on failure
 */
static int __maybe_unused
rockchip_atomic_helper_update_plane(struct drm_plane *plane,
				    struct drm_crtc *crtc,
				    struct drm_framebuffer *fb,
				    int crtc_x, int crtc_y,
				    unsigned int crtc_w, unsigned int crtc_h,
				    uint32_t src_x, uint32_t src_y,
				    uint32_t src_w, uint32_t src_h,
				    struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	struct vop_plane_state *vop_plane_state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto fail;
	}

	vop_plane_state = to_vop_plane_state(plane_state);

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		goto fail;
	drm_atomic_set_fb_for_plane(plane_state, fb);
	plane_state->crtc_x = crtc_x;
	plane_state->crtc_y = crtc_y;
	plane_state->crtc_w = crtc_w;
	plane_state->crtc_h = crtc_h;
	plane_state->src_x = src_x;
	plane_state->src_y = src_y;
	plane_state->src_w = src_w;
	plane_state->src_h = src_h;

	if (plane == crtc->cursor || vop_plane_state->async_commit)
		state->legacy_cursor_update = true;

	ret = drm_atomic_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}


/**
 * rockchip_atomic_helper_disable_plane - copy from drm_atomic_helper_disable_plane
 * be designed to support async commit at ioctl DRM_IOCTL_MODE_SETPLANE.
 *
 * @plane: plane to disable
 * @ctx: lock acquire context
 *
 * Provides a default plane disable handler using the atomic driver interface.
 *
 * RETURNS:
 * Zero on success, error code on failure
 */
static int __maybe_unused
rockchip_atomic_helper_disable_plane(struct drm_plane *plane,
				     struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	struct vop_plane_state *vop_plane_state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto fail;
	}
	vop_plane_state = to_vop_plane_state(plane_state);

	if ((plane_state->crtc && plane_state->crtc->cursor == plane) ||
	    vop_plane_state->async_commit)
		plane_state->state->legacy_cursor_update = true;

	ret = __drm_atomic_helper_disable_plane(plane, plane_state);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}

static void vop_plane_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);
}

static void vop_atomic_plane_reset(struct drm_plane *plane)
{
	struct vop_plane_state *vop_plane_state;
	struct vop_win *win = to_vop_win(plane);

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);
		vop_plane_state = to_vop_plane_state(plane->state);
		kfree(vop_plane_state);
		plane->state = NULL;
	}
	vop_plane_state = kzalloc(sizeof(*vop_plane_state), GFP_KERNEL);
	if (!vop_plane_state)
		return;

	__drm_atomic_helper_plane_reset(plane, &vop_plane_state->base);
	vop_plane_state->base.zpos = win->zpos;
	vop_plane_state->global_alpha = 0xff;
}

static struct drm_plane_state *
vop_atomic_plane_duplicate_state(struct drm_plane *plane)
{
	struct vop_plane_state *old_vop_plane_state;
	struct vop_plane_state *vop_plane_state;

	if (WARN_ON(!plane->state))
		return NULL;

	old_vop_plane_state = to_vop_plane_state(plane->state);
	vop_plane_state = kmemdup(old_vop_plane_state,
				  sizeof(*vop_plane_state), GFP_KERNEL);
	if (!vop_plane_state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane,
						  &vop_plane_state->base);

	return &vop_plane_state->base;
}

static void vop_atomic_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *state)
{
	struct vop_plane_state *vop_state = to_vop_plane_state(state);

	__drm_atomic_helper_plane_destroy_state(state);

	kfree(vop_state);
}

static int vop_atomic_plane_set_property(struct drm_plane *plane,
					 struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct rockchip_drm_private *private = plane->dev->dev_private;
	struct vop_win *win = to_vop_win(plane);
	struct vop_plane_state *plane_state = to_vop_plane_state(state);

	if (property == private->eotf_prop) {
		plane_state->eotf = val;
		return 0;
	}

	if (property == private->async_commit_prop) {
		plane_state->async_commit = val;
		return 0;
	}

	if (property == win->color_key_prop) {
		plane_state->color_key = val;
		return 0;
	}

	DRM_ERROR("failed to set vop plane property id:%d, name:%s\n",
		   property->base.id, property->name);

	return -EINVAL;
}

static int vop_atomic_plane_get_property(struct drm_plane *plane,
					 const struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t *val)
{
	struct vop_plane_state *plane_state = to_vop_plane_state(state);
	struct vop_win *win = to_vop_win(plane);
	struct rockchip_drm_private *private = plane->dev->dev_private;

	if (property == private->eotf_prop) {
		*val = plane_state->eotf;
		return 0;
	}

	if (property == private->async_commit_prop) {
		*val = plane_state->async_commit;
		return 0;
	}

	if (property == private->share_id_prop) {
		int i;
		struct drm_mode_object *obj = &plane->base;

		for (i = 0; i < obj->properties->count; i++) {
			if (obj->properties->properties[i] == property) {
				*val = obj->properties->values[i];
				return 0;
			}
		}
	}

	if (property == win->color_key_prop) {
		*val = plane_state->color_key;
		return 0;
	}

	DRM_ERROR("failed to get vop plane property id:%d, name:%s\n",
		   property->base.id, property->name);

	return -EINVAL;
}

static const struct drm_plane_funcs vop_plane_funcs = {
	.update_plane	= rockchip_atomic_helper_update_plane,
	.disable_plane	= rockchip_atomic_helper_disable_plane,
	.destroy = vop_plane_destroy,
	.reset = vop_atomic_plane_reset,
	.atomic_duplicate_state = vop_atomic_plane_duplicate_state,
	.atomic_destroy_state = vop_atomic_plane_destroy_state,
	.atomic_set_property = vop_atomic_plane_set_property,
	.atomic_get_property = vop_atomic_plane_get_property,
	.format_mod_supported = rockchip_vop_mod_supported,
};

static int vop_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return -EPERM;

	spin_lock_irqsave(&vop->irq_lock, flags);

	if (vop->version == VOP_VERSION_RK3228 || vop->version == VOP_VERSION_RK3328) {
		VOP_INTR_SET_TYPE(vop, clear, FS_FIELD_INTR, 1);
		VOP_INTR_SET_TYPE(vop, enable, FS_FIELD_INTR, 1);
	} else {
		VOP_INTR_SET_TYPE(vop, clear, FS_INTR, 1);
		VOP_INTR_SET_TYPE(vop, enable, FS_INTR, 1);
	}

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	return 0;
}

static void vop_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	if (vop->version == VOP_VERSION_RK3228 || vop->version == VOP_VERSION_RK3328)
		VOP_INTR_SET_TYPE(vop, enable, FS_FIELD_INTR, 0);
	else
		VOP_INTR_SET_TYPE(vop, enable, FS_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_crtc_cancel_pending_vblank(struct drm_crtc *crtc,
					   struct drm_file *file_priv)
{
	struct drm_device *drm = crtc->dev;
	struct vop *vop = to_vop(crtc);
	struct drm_pending_vblank_event *e;
	unsigned long flags;

	spin_lock_irqsave(&drm->event_lock, flags);
	e = vop->event;
	if (e && e->base.file_priv == file_priv) {
		vop->event = NULL;

		/* e->base.destroy(&e->base);//todo */
		file_priv->event_space += sizeof(e->event);
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);
}

static int vop_crtc_loader_protect(struct drm_crtc *crtc, bool on, void *data)
{
	struct rockchip_drm_private *private = crtc->dev->dev_private;
	struct vop *vop = to_vop(crtc);
	int sys_status = drm_crtc_index(crtc) ?
				SYS_STATUS_LCDC1 : SYS_STATUS_LCDC0;
	struct vop_win *win;

	if (on == vop->loader_protect)
		return 0;

	if (on) {
		if (vop->dclk_source) {
			struct clk *parent;

			parent = clk_get_parent(vop->dclk_source);
			if (parent) {
				if (clk_is_match(private->default_pll.pll, parent))
					vop->pll = &private->default_pll;
				else if (clk_is_match(private->hdmi_pll.pll, parent))
					vop->pll = &private->hdmi_pll;
				if (vop->pll)
					vop->pll->use_count++;
			}
		}

		rockchip_set_system_status(sys_status);
		vop_initial(crtc);
		if (crtc->primary) {
			win = to_vop_win(crtc->primary);
			if (VOP_WIN_GET(vop, win, enable))
				vop->enabled_win_mask |= BIT(win->win_id);
		}
		drm_crtc_vblank_on(crtc);
		vop->loader_protect = true;
	} else {
		vop_crtc_atomic_disable(crtc, NULL);

		if (vop->dclk_source && vop->pll) {
			vop->pll->use_count--;
			vop->pll = NULL;
		}
		vop->loader_protect = false;
	}

	return 0;
}

#define DEBUG_PRINT(args...) \
		do { \
			if (s) \
				seq_printf(s, args); \
			else \
				pr_err(args); \
		} while (0)

static int vop_plane_info_dump(struct seq_file *s, struct drm_plane *plane)
{
	struct vop_win *win = to_vop_win(plane);
	struct drm_plane_state *state = plane->state;
	struct vop_plane_state *pstate = to_vop_plane_state(state);
	struct drm_rect *src, *dest;
	struct drm_framebuffer *fb = state->fb;
	int i;
	struct drm_gem_object *obj;
	struct rockchip_gem_object *rk_obj;
	dma_addr_t fb_addr;

	DEBUG_PRINT("    win%d-%d: %s\n", win->win_id, win->area_id,
		    state->crtc ? "ACTIVE" : "DISABLED");
	if (!fb)
		return 0;

	src = &pstate->src;
	dest = &pstate->dest;

	DEBUG_PRINT("\tformat: %p4cc%s%s[%d] color-encoding[%d] color-range[%d]\n",
		    &fb->format->format,
		    rockchip_afbc(plane, state->fb->modifier) ? "[AFBC]" : "",
		    pstate->eotf ? " HDR" : " SDR", pstate->eotf,
		    state->color_encoding, state->color_range);
	DEBUG_PRINT("\tcsc: y2r[%d] r2r[%d] r2y[%d] csc mode[%d]\n",
		    pstate->y2r_en, pstate->r2r_en, pstate->r2y_en,
		    pstate->csc_mode);
	DEBUG_PRINT("\tzpos: %d\n", pstate->zpos);
	DEBUG_PRINT("\tsrc: pos[%dx%d] rect[%dx%d]\n", src->x1 >> 16,
		    src->y1 >> 16, drm_rect_width(src) >> 16,
		    drm_rect_height(src) >> 16);
	DEBUG_PRINT("\tdst: pos[%dx%d] rect[%dx%d]\n", dest->x1, dest->y1,
		    drm_rect_width(dest), drm_rect_height(dest));

	for (i = 0; i < fb->format->num_planes; i++) {
		obj = fb->obj[0];
		rk_obj = to_rockchip_obj(obj);
		fb_addr = rk_obj->dma_addr + fb->offsets[0];

		DEBUG_PRINT("\tbuf[%d]: addr: %pad pitch: %d offset: %d\n",
			    i, &fb_addr, fb->pitches[i], fb->offsets[i]);
	}

	return 0;
}

static void vop_dump_connector_on_crtc(struct drm_crtc *crtc, struct seq_file *s)
{
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;

	drm_connector_list_iter_begin(crtc->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (crtc->state->connector_mask & drm_connector_mask(connector))
			DEBUG_PRINT("    Connector: %s\n", connector->name);

	}
	drm_connector_list_iter_end(&conn_iter);
}

static int vop_crtc_debugfs_dump(struct drm_crtc *crtc, struct seq_file *s)
{
	struct vop *vop = to_vop(crtc);
	struct drm_crtc_state *crtc_state = crtc->state;
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	struct rockchip_crtc_state *state = to_rockchip_crtc_state(crtc->state);
	bool interlaced = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);
	struct drm_plane *plane;
	int i;

	DEBUG_PRINT("VOP [%s]: %s\n", dev_name(vop->dev),
		    crtc_state->active ? "ACTIVE" : "DISABLED");

	if (!crtc_state->active)
		return 0;

	vop_dump_connector_on_crtc(crtc, s);
	DEBUG_PRINT("\tbus_format[%x]: %s\n", state->bus_format,
		    drm_get_bus_format_name(state->bus_format));
	DEBUG_PRINT("\toverlay_mode[%d] output_mode[%x]",
		    state->yuv_overlay, state->output_mode);
	DEBUG_PRINT("color-encoding[%d] color-range[%d]\n",
		    state->color_encoding, state->color_range);
	DEBUG_PRINT("    Display mode: %dx%d%s%d\n",
		    mode->hdisplay, mode->vdisplay, interlaced ? "i" : "p",
		    drm_mode_vrefresh(mode));
	DEBUG_PRINT("\tdclk[%d kHz] real_dclk[%d kHz] aclk[%ld kHz] type[%x] flag[%x]\n",
		    mode->clock, mode->crtc_clock, clk_get_rate(vop->aclk) / 1000,
		    mode->type, mode->flags);
	DEBUG_PRINT("\tH: %d %d %d %d\n", mode->hdisplay, mode->hsync_start,
		    mode->hsync_end, mode->htotal);
	DEBUG_PRINT("\tV: %d %d %d %d\n", mode->vdisplay, mode->vsync_start,
		    mode->vsync_end, mode->vtotal);

	for (i = 0; i < vop->num_wins; i++) {
		plane = &vop->win[i].base;
		vop_plane_info_dump(s, plane);
	}
	DEBUG_PRINT("    post: sdr2hdr[%d] hdr2sdr[%d]\n",
		    state->hdr.sdr2hdr_state.bt1886eotf_post_conv_en,
		    state->hdr.hdr2sdr_en);
	DEBUG_PRINT("    pre : sdr2hdr[%d]\n",
		    state->hdr.sdr2hdr_state.bt1886eotf_pre_conv_en);
	DEBUG_PRINT("    post CSC: r2y[%d] y2r[%d] CSC mode[%d]\n",
		    state->post_r2y_en, state->post_y2r_en,
		    state->post_csc_mode);

	return 0;
}

static void vop_crtc_regs_dump(struct drm_crtc *crtc, struct seq_file *s)
{
	struct vop *vop = to_vop(crtc);
	struct drm_crtc_state *crtc_state = crtc->state;
	int dump_len = vop->len > 0x400 ? 0x400 : vop->len;
	resource_size_t offset_addr;
	int i;

	if (!crtc_state->active)
		return;

	offset_addr = vop->res->start;
	for (i = 0; i < dump_len; i += 16) {
		DEBUG_PRINT("0x%08x: %08x %08x %08x %08x\n", (u32)offset_addr + i,
			    vop_readl(vop, i), vop_readl(vop, i + 4),
			    vop_readl(vop, i + 8), vop_readl(vop, i + 12));
	}
}

static int vop_crtc_regs_write(struct drm_crtc *crtc, phys_addr_t address, u32 val)
{
	struct vop *vop = to_vop(crtc);
	struct drm_crtc_state *crtc_state = crtc->state;
	void __iomem *regs;
	u32 offset = 0;

	if (!crtc_state->active) {
		DRM_DEV_ERROR(vop->dev, "VOP is disabled");
		return -EINVAL;
	}

	if (vop->res && address >= vop->res->start &&
	    address < vop->res->end) {
		regs = vop->regs;
		offset = address - vop->res->start;
	} else if (vop->lut_res && address >= vop->lut_res->start &&
		   address < vop->lut_res->end) {
		regs = vop->lut_regs;
		offset = address - vop->lut_res->start;
	} else {
		DRM_ERROR("unsupported address: %pa\n", &address);
		return -ENXIO;
	}
	writel(val, regs + offset);

	return 0;
}

static int vop_gamma_show(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct vop *vop = node->info_ent->data;
	int i;

	if (!vop->lut || !vop->lut_active || !vop->lut_regs)
		return 0;

	for (i = 0; i < vop->lut_len; i++) {
		if (i % 8 == 0)
			DEBUG_PRINT("\n");
		DEBUG_PRINT("0x%08x ", vop->lut[i]);
	}
	DEBUG_PRINT("\n");

	return 0;
}

#undef DEBUG_PRINT

static struct drm_info_list vop_debugfs_files[] = {
	{ "gamma_lut", vop_gamma_show, 0, NULL },
};

static int vop_crtc_late_register(struct drm_crtc *crtc)
{
	struct drm_minor *minor = crtc->dev->primary;
	struct vop *vop = to_vop(crtc);
	int ret, i;

	vop->debugfs = debugfs_create_dir(dev_name(vop->dev),
					  minor->debugfs_root);

	if (!vop->debugfs)
		return -ENOMEM;

	vop->debugfs_files = kmemdup(vop_debugfs_files,
				     sizeof(vop_debugfs_files),
				     GFP_KERNEL);
	if (!vop->debugfs_files) {
		ret = -ENOMEM;
		goto remove;
	}
#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
	rockchip_drm_add_dump_buffer(crtc, vop->debugfs);
	rockchip_drm_debugfs_add_regs_write(crtc, vop->debugfs);
	rockchip_drm_debugfs_add_color_bar(crtc, vop->debugfs);
	rockchip_drm_debugfs_add_dclk_rate(crtc, vop->debugfs);
#endif
	for (i = 0; i < ARRAY_SIZE(vop_debugfs_files); i++)
		vop->debugfs_files[i].data = vop;

	drm_debugfs_create_files(vop->debugfs_files, ARRAY_SIZE(vop_debugfs_files),
				 vop->debugfs, minor);

	return 0;
remove:
	debugfs_remove(vop->debugfs);
	vop->debugfs = NULL;
	return ret;
}

static void vop_crtc_early_unregister(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);

	debugfs_remove_recursive(vop->debugfs);
}

static enum drm_mode_status
vop_crtc_mode_valid(struct drm_crtc *crtc, const struct drm_display_mode *mode)
{
	struct vop *vop = to_vop(crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	const struct vop_data *vop_data = vop->data;
	int request_clock = mode->clock;
	int clock;

	if (mode->hdisplay > vop_data->max_output.width)
		return MODE_BAD_HVALUE;

	if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
	    (vop->version == VOP_VERSION_RK3288 || vop->version == VOP_VERSION_RK3288W ||
	     vop->version == VOP_VERSION_RK3368))
		return MODE_BAD;

	/*
	 * Dclk need to be double if BT656 interface and vop version >= 2.12.
	 * That is RV1126/RV1106/RK3576_LITE/RK3506/RV1126B
	 */
	if (mode->flags & DRM_MODE_FLAG_DBLCLK ||
	    (vop->version >= VOP_VERSION_RV1106 && vop->version <= VOP_VERSION_RK3288 &&
	     s->output_if & VOP_OUTPUT_IF_BT656))
		request_clock *= 2;
	clock = clk_round_rate(vop->dclk, request_clock * 1000) / 1000;

	/*
	 * Hdmi or DisplayPort request a Accurate clock.
	 */
	if (s->output_type == DRM_MODE_CONNECTOR_HDMIA ||
	    s->output_type == DRM_MODE_CONNECTOR_DisplayPort)
		if (clock != request_clock)
			return MODE_CLOCK_RANGE;

	return MODE_OK;
}

struct vop_bandwidth {
	size_t bandwidth;
	int y1;
	int y2;
};

static int vop_bandwidth_cmp(const void *a, const void *b)
{
	struct vop_bandwidth *pa = (struct vop_bandwidth *)a;
	struct vop_bandwidth *pb = (struct vop_bandwidth *)b;

	return pa->y1 - pb->y2;
}

static size_t vop_plane_line_bandwidth(struct drm_plane_state *pstate)
{
	struct vop_plane_state *vop_plane_state = to_vop_plane_state(pstate);
	struct vop_win *win = to_vop_win(pstate->plane);
	struct drm_crtc *crtc = pstate->crtc;
	struct vop *vop = to_vop(crtc);
	struct drm_framebuffer *fb = pstate->fb;
	struct drm_rect *dest = &vop_plane_state->dest;
	struct drm_rect *src = &vop_plane_state->src;
	int bpp = fb->format->cpp[0] << 3;
	int src_width = drm_rect_width(src) >> 16;
	int src_height = drm_rect_height(src) >> 16;
	int dest_width = drm_rect_width(dest);
	int dest_height = drm_rect_height(dest);
	int vskiplines = scl_get_vskiplines(src_height, dest_height);
	size_t bandwidth;

	if (src_width <= 0 || src_height <= 0 || dest_width <= 0 ||
	    dest_height <= 0)
		return 0;

	bandwidth = src_width * bpp / 8;

	bandwidth = bandwidth * src_width / dest_width;
	bandwidth = bandwidth * src_height / dest_height;
	if (vskiplines == 2 && VOP_WIN_SCL_EXT_SUPPORT(vop, win, vsd_yrgb_gt2))
		bandwidth /= 2;
	else if (vskiplines == 4 &&
		 VOP_WIN_SCL_EXT_SUPPORT(vop, win, vsd_yrgb_gt4))
		bandwidth /= 4;

	return bandwidth;
}

static u64 vop_calc_max_bandwidth(struct vop_bandwidth *bw, int start,
				  int count, int y2)
{
	u64 max_bandwidth = 0;
	int i;

	for (i = start; i < count; i++) {
		u64 bandwidth = 0;

		if (bw[i].y1 > y2)
			continue;
		bandwidth = bw[i].bandwidth;
		bandwidth += vop_calc_max_bandwidth(bw, i + 1, count,
						    min(bw[i].y2, y2));

		if (bandwidth > max_bandwidth)
			max_bandwidth = bandwidth;
	}

	return max_bandwidth;
}

static size_t vop_crtc_bandwidth(struct drm_crtc *crtc,
				 struct drm_crtc_state *crtc_state,
				 struct dmcfreq_vop_info *vop_bw_info)
{
	struct drm_display_mode *adjusted_mode = &crtc_state->adjusted_mode;
	u16 htotal = adjusted_mode->crtc_htotal;
	u16 vdisplay = adjusted_mode->crtc_vdisplay;
	int clock = adjusted_mode->crtc_clock;
	struct vop_plane_state *vop_plane_state;
	struct drm_plane_state *pstate;
	struct vop_bandwidth *pbandwidth;
	struct drm_plane *plane;
	u64 line_bw_mbyte = 0;
	int cnt = 0, plane_num = 0;
	struct drm_atomic_state *state = crtc_state->state;

	if (!htotal || !vdisplay)
		return 0;

	drm_atomic_crtc_state_for_each_plane(plane, crtc_state)
		plane_num++;

	vop_bw_info->plane_num += plane_num;
	pbandwidth = kmalloc_array(plane_num, sizeof(*pbandwidth),
				   GFP_KERNEL);
	if (!pbandwidth)
		return -ENOMEM;

	drm_atomic_crtc_state_for_each_plane(plane, crtc_state) {
		int act_w, act_h, cpp, afbc_fac;

		pstate = drm_atomic_get_existing_plane_state(state, plane);
		if (pstate->crtc != crtc || !pstate->fb)
			continue;

		/* This is an empirical value, if it's afbc format, the frame buffer size div 2 */
		afbc_fac = rockchip_afbc(plane, pstate->fb->modifier) ? 2 : 1;

		vop_plane_state = to_vop_plane_state(pstate);
		pbandwidth[cnt].y1 = vop_plane_state->dest.y1;
		pbandwidth[cnt].y2 = vop_plane_state->dest.y2;
		pbandwidth[cnt++].bandwidth = vop_plane_line_bandwidth(pstate) / afbc_fac;

		act_w = drm_rect_width(&pstate->src) >> 16;
		act_h = drm_rect_height(&pstate->src) >> 16;
		cpp = pstate->fb->format->cpp[0];

		vop_bw_info->frame_bw_mbyte += act_w * act_h / 1000 * cpp * drm_mode_vrefresh(adjusted_mode) / 1000;

	}

	sort(pbandwidth, cnt, sizeof(pbandwidth[0]), vop_bandwidth_cmp, NULL);

	line_bw_mbyte = vop_calc_max_bandwidth(pbandwidth, 0, cnt, vdisplay);
	kfree(pbandwidth);
	/*
	 * line_bandwidth(MB/s)
	 *    = line_bandwidth / line_time
	 *    = line_bandwidth(Byte) * clock(KHZ) / 1000 / htotal
	 */
	line_bw_mbyte *= clock;
	do_div(line_bw_mbyte, htotal * 1000);
	vop_bw_info->line_bw_mbyte = line_bw_mbyte;

	return vop_bw_info->line_bw_mbyte;
}

static void vop_crtc_close(struct drm_crtc *crtc)
{
	struct vop *vop = NULL;

	if (!crtc)
		return;
	vop = to_vop(crtc);
	mutex_lock(&vop->vop_lock);
	if (!vop->is_enabled) {
		mutex_unlock(&vop->vop_lock);
		return;
	}

	vop_disable_all_planes(vop);
	mutex_unlock(&vop->vop_lock);
}

static u32 vop_mode_done(struct vop *vop)
{
	return VOP_CTRL_GET(vop, out_mode);
}

static void vop_set_out_mode(struct vop *vop, u32 mode)
{
	int ret;
	u32 val;

	VOP_CTRL_SET(vop, out_mode, mode);
	vop_cfg_done(vop);
	ret = readx_poll_timeout(vop_mode_done, vop, val, val == mode,
				 1000, 500 * 1000);
	if (ret)
		dev_err(vop->dev, "wait mode 0x%x timeout\n", mode);
}

static void vop_mcu_bypass_mode_setup(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	const struct rockchip_mcu_timing *mcu_timing = &vop->data->mcu_bypass_cfg->timing;

	/*
	 * If mcu_hold_mode is 1, set 1 to mcu_frame_st will
	 * refresh one frame from ddr. So mcu_frame_st is needed
	 * to be initialized as 0.
	 */
	VOP_CTRL_SET(vop, mcu_frame_st, 0);
	VOP_CTRL_SET(vop, mcu_clk_sel, 1);
	VOP_CTRL_SET(vop, mcu_type, 1);

	VOP_CTRL_SET(vop, mcu_hold_mode, 1);
	/*
	 * Use user-defined mcu bypass timing if mcu-bypass-timing
	 * node has been found in dts, otherwise setup the default
	 * timing which can meet the read/write timing requirements
	 * of most mcu panel.
	 */
	if (vop->mcu_bypass_timing.mcu_pix_total) {
		VOP_CTRL_SET(vop, mcu_pix_total, vop->mcu_bypass_timing.mcu_pix_total);
		VOP_CTRL_SET(vop, mcu_cs_pst, vop->mcu_bypass_timing.mcu_cs_pst);
		VOP_CTRL_SET(vop, mcu_cs_pend, vop->mcu_bypass_timing.mcu_cs_pend);
		VOP_CTRL_SET(vop, mcu_rw_pst, vop->mcu_bypass_timing.mcu_rw_pst);
		VOP_CTRL_SET(vop, mcu_rw_pend, vop->mcu_bypass_timing.mcu_rw_pend);
	} else {
		VOP_CTRL_SET(vop, mcu_pix_total, mcu_timing->mcu_pix_total);
		VOP_CTRL_SET(vop, mcu_cs_pst, mcu_timing->mcu_cs_pst);
		VOP_CTRL_SET(vop, mcu_cs_pend, mcu_timing->mcu_cs_pend);
		VOP_CTRL_SET(vop, mcu_rw_pst, mcu_timing->mcu_rw_pst);
		VOP_CTRL_SET(vop, mcu_rw_pend, mcu_timing->mcu_rw_pend);

		clk_set_rate(vop->dclk, vop->data->mcu_bypass_cfg->dclk_rate);
	}
}

static void vop_mcu_mode_setup(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);

	/*
	 * If mcu_hold_mode is 1, set 1 to mcu_frame_st will
	 * refresh one frame from ddr. So mcu_frame_st is needed
	 * to be initialized as 0.
	 */
	VOP_CTRL_SET(vop, mcu_frame_st, 0);
	VOP_CTRL_SET(vop, mcu_clk_sel, 1);
	VOP_CTRL_SET(vop, mcu_type, 1);

	VOP_CTRL_SET(vop, mcu_hold_mode, 1);
	VOP_CTRL_SET(vop, mcu_pix_total, vop->mcu_timing.mcu_pix_total);
	VOP_CTRL_SET(vop, mcu_cs_pst, vop->mcu_timing.mcu_cs_pst);
	VOP_CTRL_SET(vop, mcu_cs_pend, vop->mcu_timing.mcu_cs_pend);
	VOP_CTRL_SET(vop, mcu_rw_pst, vop->mcu_timing.mcu_rw_pst);
	VOP_CTRL_SET(vop, mcu_rw_pend, vop->mcu_timing.mcu_rw_pend);
}

static void vop_crtc_send_mcu_cmd(struct drm_crtc *crtc, u32 type, u32 value)
{
	struct drm_display_mode *adjusted_mode;
	struct vop *vop = NULL;
	uint32_t val = 0;

	if (!crtc)
		return;

	vop = to_vop(crtc);
	adjusted_mode = &crtc->state->adjusted_mode;

	if (vop->data->mcu_bypass_cfg) {
		/*
		 * 1.set mcu bypass mode timing.
		 * 2.set dclk rate to 150M.
		 */
		if ((type == MCU_SETBYPASS) && value)
			vop_mcu_bypass_mode_setup(crtc);
	}

	mutex_lock(&vop->vop_lock);
	if (vop && vop->is_enabled) {
		switch (type) {
		case MCU_WRCMD:
			VOP_CTRL_SET(vop, mcu_force_rdn, 1);
			VOP_CTRL_SET(vop, mcu_rs, 0);
			VOP_CTRL_SET(vop, mcu_rw_bypass_port, value);
			VOP_CTRL_SET(vop, mcu_rs, 1);
			break;
		case MCU_WRDATA:
			VOP_CTRL_SET(vop, mcu_force_rdn, 1);
			VOP_CTRL_SET(vop, mcu_rs, 1);
			VOP_CTRL_SET(vop, mcu_rw_bypass_port, value);
			break;
		case MCU_RDDATA:
			VOP_CTRL_SET(vop, mcu_force_rdn, 0);
			VOP_CTRL_SET(vop, mcu_rs, 1);
			val = VOP_CTRL_GET(vop, mcu_rw_bypass_port);
			DRM_DEBUG_DRIVER("mcu read reg[0x%02x] = 0x%02x", value, val);
			break;
		case MCU_SETBYPASS:
			VOP_CTRL_SET(vop, mcu_bypass, value ? 1 : 0);
			if (!value)
				VOP_CTRL_SET(vop, mcu_force_rdn, 1);
			break;
		default:
			break;
		}
	}
	mutex_unlock(&vop->vop_lock);

	if (vop->data->mcu_bypass_cfg) {
		/*
		 * 1.restore mcu data mode timing.
		 * 2.restore dclk rate to crtc_clock.
		 */
		if ((type == MCU_SETBYPASS) && !value) {
			vop_mcu_mode_setup(crtc);
			clk_set_rate(vop->dclk, adjusted_mode->crtc_clock * 1000);
		}
	}
}

static int vop_crtc_wait_vact_end(struct drm_crtc *crtc, unsigned int mstimeout)
{
	struct vop *vop = to_vop(crtc);
	unsigned long jiffies_left;
	int ret = 0;

	if (!vop->is_enabled)
		return -ENODEV;

	mutex_lock(&vop->vop_lock);

	if (vop_line_flag_irq_is_enabled(vop)) {
		ret = -EBUSY;
		goto out;
	}

	reinit_completion(&vop->line_flag_completion);
	vop_line_flag_irq_enable(vop);

	jiffies_left = wait_for_completion_timeout(&vop->line_flag_completion,
						   msecs_to_jiffies(mstimeout));
	vop_line_flag_irq_disable(vop);

	if (jiffies_left == 0) {
		DRM_DEV_ERROR(vop->dev, "timeout waiting for lineflag IRQ\n");
		ret = -ETIMEDOUT;
		goto out;
	}

out:
	mutex_unlock(&vop->vop_lock);
	return ret;
}

static void vop_crtc_te_handler(struct drm_crtc *crtc)
{
	struct vop *vop;

	if (!crtc)
		return;

	vop = to_vop(crtc);

	if (vop->mcu_timing.mcu_pix_total) {
		VOP_CTRL_SET(vop, mcu_frame_st, 1);
	} else {
		/*
		 * For software TE mode, we register a gpio IRQ to respond to
		 * the TE signal from the panel. If the TE signal is detected
		 * via gpio, a new frame will be sent to the panel for display
		 * only by controlling the edpi_wms_fs bit.
		 *
		 * As the IC design, the VOP will only refresh one new frame
		 * of image when the ​​edpi_wms_fs​​ bit, which can take effect
		 * immediately, is first written with ​​1​ and then cleared to 0​​.
		 * If only written to ​​1​​, it will result in ​​two frames being
		 * refreshed​​ instead.
		 */
		VOP_CTRL_SET(vop, edpi_wms_fs, 1);
		VOP_CTRL_SET(vop, edpi_wms_fs, 0);
	}
}

#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
static int vop_crtc_set_color_bar(struct drm_crtc *crtc, enum rockchip_color_bar_mode mode)
{
	struct vop *vop = to_vop(crtc);
	int ret = 0;

	if (!crtc->state->active) {
		DRM_INFO("VOP disabled\n");
		return -EINVAL;
	}

	if (!VOP_CTRL_SUPPORT(vop, color_bar_en)) {
		DRM_INFO("color bar not support\n");
		return -EINVAL;
	}

	switch (mode) {
	case ROCKCHIP_COLOR_BAR_OFF:
		VOP_CTRL_SET(vop, color_bar_en, 0);
		vop_cfg_done(vop);
		break;
	case ROCKCHIP_COLOR_BAR_HORIZONTAL:
		VOP_CTRL_SET(vop, color_bar_mode, 0);
		VOP_CTRL_SET(vop, color_bar_en, 1);
		vop_cfg_done(vop);
		break;
	case ROCKCHIP_COLOR_BAR_VERTICAL:
		VOP_CTRL_SET(vop, color_bar_mode, 1);
		VOP_CTRL_SET(vop, color_bar_en, 1);
		vop_cfg_done(vop);
		break;
	default:
		DRM_INFO("Unsupported color bar mode\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned long vop_crtc_get_dclk_rate(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	unsigned long rate, count;

	if (!VOP_CTRL_SUPPORT(vop, calc_dclk_cnt))
		return 0;

	VOP_CTRL_SET(vop, calc_clk_en, 1);
	usleep_range(500, 1000);
	count = VOP_CTRL_GET(vop, calc_dclk_cnt);
	rate = clk_get_rate(vop->aclk);

	/* calc_dclk_cnt is the count number when aclk counts to 10000 */
	rate = rate / 10000 * count;

	VOP_CTRL_SET(vop, calc_clk_en, 0);
	return rate;
}
#endif

static const struct rockchip_crtc_funcs private_crtc_funcs = {
	.loader_protect = vop_crtc_loader_protect,
	.cancel_pending_vblank = vop_crtc_cancel_pending_vblank,
	.debugfs_dump = vop_crtc_debugfs_dump,
	.active_regs_dump = vop_crtc_regs_dump,
	.regs_dump = vop_crtc_regs_dump,
	.regs_write = vop_crtc_regs_write,
	.bandwidth = vop_crtc_bandwidth,
	.crtc_close = vop_crtc_close,
	.crtc_send_mcu_cmd = vop_crtc_send_mcu_cmd,
	.wait_vact_end = vop_crtc_wait_vact_end,
	.te_handler = vop_crtc_te_handler,
#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
	.crtc_set_color_bar = vop_crtc_set_color_bar,
	.crtc_get_dclk_rate = vop_crtc_get_dclk_rate,
#endif
};

static bool vop_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;
	struct drm_crtc_state *new_crtc_state = container_of(mode, struct drm_crtc_state, mode);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(new_crtc_state);
	unsigned long rate;

	if (mode->hdisplay > vop_data->max_output.width)
		return false;

	drm_mode_set_crtcinfo(adj_mode,
			      CRTC_INTERLACE_HALVE_V | CRTC_STEREO_DOUBLE);

	/*
	 * Dclk need to be double if BT656 interface and vop version >= 2.12.
	 * That is RV1126/RV1106/RK3576_LITE/RK3506/RV1126B
	 */
	if (mode->flags & DRM_MODE_FLAG_DBLCLK ||
	    (vop->version >= VOP_VERSION_RV1106 && vop->version <= VOP_VERSION_RK3288 &&
	     s->output_if & VOP_OUTPUT_IF_BT656))
		adj_mode->crtc_clock *= 2;

	if (s->output_if & VOP_OUTPUT_IF_RGB)
		adj_mode->crtc_clock *= rockchip_drm_get_cycles_per_pixel(s->bus_format) *
					(vop->mcu_timing.mcu_pix_total + 1);

	/*
	 * Clock craziness.
	 *
	 * Key points:
	 *
	 * - DRM works in kHz.
	 * - Clock framework works in Hz.
	 * - Rockchip's clock driver picks the clock rate that is the
	 *   same _OR LOWER_ than the one requested.
	 *
	 * Action plan:
	 *
	 * 1. Try to set the exact rate first, and confirm the clock framework
	 *    can provide it.
	 *
	 * 2. If the clock framework cannot provide the exact rate, we should
	 *    add 999 Hz to the requested rate.  That way if the clock we need
	 *    is 60000001 Hz (~60 MHz) and DRM tells us to make 60000 kHz then
	 *    the clock framework will actually give us the right clock.
	 *
	 * 3. Get the clock framework to round the rate for us to tell us
	 *    what it will actually make.
	 *
	 * 4. Store the rounded up rate so that we don't need to worry about
	 *    this in the actual clk_set_rate().
	 */
	rate = clk_round_rate(vop->dclk, adj_mode->crtc_clock * 1000);
	if (rate / 1000 != adj_mode->crtc_clock)
		rate = clk_round_rate(vop->dclk,
				      adj_mode->crtc_clock * 1000 + 999);
	adj_mode->crtc_clock = DIV_ROUND_UP(rate, 1000);

	return true;
}

static void vop_dither_setup(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);

	/*
	 * VOP MCU interface can't work right when dither enabled.
	 * (1) the MCU CMD will be treated as data then changed by dither algorithm
	 * (2) the dither algorithm works wrong in mcu mode
	 */
	if (vop->mcu_timing.mcu_pix_total)
		return;

	switch (s->bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		VOP_CTRL_SET(vop, dither_down_en, 1);
		VOP_CTRL_SET(vop, dither_down_mode, RGB888_TO_RGB565);
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
	case MEDIA_BUS_FMT_RGB666_3X6:
		VOP_CTRL_SET(vop, dither_down_en, 1);
		VOP_CTRL_SET(vop, dither_down_mode, RGB888_TO_RGB666);
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
		VOP_CTRL_SET(vop, dither_down_en, 0);
		VOP_CTRL_SET(vop, pre_dither_down_en, 1);
		break;
	case MEDIA_BUS_FMT_YUV10_1X30:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		VOP_CTRL_SET(vop, dither_down_en, 0);
		VOP_CTRL_SET(vop, pre_dither_down_en, 0);
		break;
	case MEDIA_BUS_FMT_RGB888_3X8:
	case MEDIA_BUS_FMT_RGB888_DUMMY_4X8:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
	default:
		VOP_CTRL_SET(vop, dither_down_en, 0);
		VOP_CTRL_SET(vop, pre_dither_down_en, 0);
		break;
	}

	VOP_CTRL_SET(vop, pre_dither_down_en,
		     s->output_mode == ROCKCHIP_OUT_MODE_AAAA ? 0 : 1);
	VOP_CTRL_SET(vop, dither_down_sel, DITHER_DOWN_ALLEGRO);
}

static void vop_update_csc(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);
	u32 val;

	/*
	 * When using BT656, set RV1126/RV1106/RK3576_LITE/RK3506/RV1126B to P8888 mode.
	 */
	if ((s->output_mode == ROCKCHIP_OUT_MODE_AAAA &&
	     !(vop->data->feature & VOP_FEATURE_OUTPUT_10BIT)) ||
	    (vop->version >= VOP_VERSION_RV1106 && vop->version <= VOP_VERSION_RK3288 &&
	     s->output_if & VOP_OUTPUT_IF_BT656))
		s->output_mode = ROCKCHIP_OUT_MODE_P888;

	if (is_uv_swap(s->bus_format, s->output_mode) ||
	    is_rb_swap(s->bus_format, s->output_mode))
		VOP_CTRL_SET(vop, dsp_rb_swap, 1);
	else
		VOP_CTRL_SET(vop, dsp_data_swap, 0);

	/*
	 * For RK3576 vopl, rg_swap and rb_swap need to be enabled in
	 * YUV444 bus_format.
	 */
	if (vop->version == VOP_VERSION_RK3576_LITE && s->bus_format == MEDIA_BUS_FMT_YUV8_1X24)
		VOP_CTRL_SET(vop, dsp_data_swap, DSP_RG_SWAP | DSP_RB_SWAP);

	VOP_CTRL_SET(vop, out_mode, s->output_mode);

	vop_dither_setup(crtc);
	VOP_CTRL_SET(vop, dclk_ddr,
		     s->output_mode == ROCKCHIP_OUT_MODE_YUV420 ? 1 : 0);
	VOP_CTRL_SET(vop, hdmi_dclk_out_en,
		     s->output_mode == ROCKCHIP_OUT_MODE_YUV420 ? 1 : 0);

	VOP_CTRL_SET(vop, overlay_mode, s->yuv_overlay);
	VOP_CTRL_SET(vop, dsp_out_yuv, is_yuv_output(s->bus_format));

	/*
	 * Background color is 10bit depth if vop version >= 3.5
	 * That is RK3399/RK3228/RK3328
	 */
	if (!is_yuv_output(s->bus_format))
		val = 0;
	else if (vop->version == VOP_VERSION_RK3576_LITE)
		val = 0;
	else if (vop->version == VOP_VERSION_RK3328 && s->hdr.pre_overlay)
		val = 0;
	else if (VOP_MAJOR(vop->version) == 3 && VOP_MINOR(vop->version) >= 5)
		val = 0x20010200;
	else
		val = 0x801080;
	VOP_CTRL_SET(vop, dsp_background, val);
}

/*
 * if adjusted mode update, return true, else return false
 */
static bool vop_crtc_mode_update(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	u16 hsync_len = adjusted_mode->crtc_hsync_end -
				adjusted_mode->crtc_hsync_start;
	u16 hdisplay = adjusted_mode->crtc_hdisplay;
	u16 htotal = adjusted_mode->crtc_htotal;
	u16 hact_st = adjusted_mode->crtc_htotal -
				adjusted_mode->crtc_hsync_start;
	u16 hact_end = hact_st + hdisplay;
	u16 vdisplay = adjusted_mode->crtc_vdisplay;
	u16 vtotal = adjusted_mode->crtc_vtotal;
	u16 vsync_len = adjusted_mode->crtc_vsync_end -
				adjusted_mode->crtc_vsync_start;
	u16 vact_st = adjusted_mode->crtc_vtotal -
				adjusted_mode->crtc_vsync_start;
	u16 vact_end = vact_st + vdisplay;
	u32 htotal_sync = htotal << 16 | hsync_len;
	u32 hactive_st_end = hact_st << 16 | hact_end;
	u32 vtotal_sync = vtotal << 16 | vsync_len;
	u32 vactive_st_end = vact_st << 16 | vact_end;
	u32 crtc_clock = adjusted_mode->crtc_clock * 100;

	if (htotal_sync != VOP_CTRL_GET(vop, htotal_pw) ||
	    hactive_st_end != VOP_CTRL_GET(vop, hact_st_end) ||
	    vtotal_sync != VOP_CTRL_GET(vop, vtotal_pw) ||
	    vactive_st_end != VOP_CTRL_GET(vop, vact_st_end) ||
	    crtc_clock != clk_get_rate(vop->dclk))
		return true;

	return false;
}

static void vop_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_state = drm_atomic_get_old_crtc_state(state,
									 crtc);
	struct vop *vop = to_vop(crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	u16 hsync_len = adjusted_mode->crtc_hsync_end - adjusted_mode->crtc_hsync_start;
	u16 hdisplay = adjusted_mode->crtc_hdisplay;
	u16 htotal = adjusted_mode->crtc_htotal;
	u16 hact_st = adjusted_mode->crtc_htotal - adjusted_mode->crtc_hsync_start;
	u16 hact_end = hact_st + hdisplay;
	u16 vdisplay = adjusted_mode->crtc_vdisplay;
	u16 vtotal = adjusted_mode->crtc_vtotal;
	u16 vsync_len = adjusted_mode->crtc_vsync_end - adjusted_mode->crtc_vsync_start;
	u16 vact_st = adjusted_mode->crtc_vtotal - adjusted_mode->crtc_vsync_start;
	u16 vact_end = vact_st + vdisplay;
	int sys_status = drm_crtc_index(crtc) ?
				SYS_STATUS_LCDC1 : SYS_STATUS_LCDC0;
	uint32_t val;
	int act_end;
	bool interlaced = !!(adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE);
	int for_ddr_freq = 0;
	bool dclk_inv, yc_swap = false;

	if (old_state && old_state->self_refresh_active) {
		if (vop->aclk_rate_reset)
			clk_set_rate(vop->aclk, vop->aclk_rate);
		vop->aclk_rate_reset = false;
		if (vop->mcu_timing.mcu_pix_total)
			VOP_CTRL_SET(vop, mcu_hold_mode, 0);

		return;
	}

	rockchip_set_system_status(sys_status);
	rockchip_request_late_resume();
	vop_lock(vop);
	DRM_DEV_INFO(vop->dev, "Update mode to %dx%d%s%d, type: %d\n",
		     hdisplay, vdisplay, interlaced ? "i" : "p",
		     drm_mode_vrefresh(adjusted_mode), s->output_type);
	vop_initial(crtc);
	vop_disable_allwin(vop);
	VOP_CTRL_SET(vop, standby, 0);
	s->mode_update = vop_crtc_mode_update(crtc);
	if (s->mode_update)
		vop_disable_all_planes(vop);
	/*
	 * restore the lut table.
	 */
	if (vop->lut_active)
		vop_crtc_load_lut(crtc);

	if (s->data_map_mode != -1) {
		if (s->output_if & VOP_OUTPUT_IF_BT1120)
			VOP_CTRL_SET(vop, bt1120_data_map_mode, s->data_map_mode);
		else if (s->output_if & VOP_OUTPUT_IF_BT656)
			VOP_CTRL_SET(vop, bt656_data_map_mode, s->data_map_mode);
		else if (vop->mcu_timing.mcu_pix_total)
			VOP_CTRL_SET(vop, mcu_data_map_mode, s->data_map_mode);
	}

	if (vop->mcu_timing.mcu_pix_total) {
		/*
		 * For RK3576_LITE/RK3506/RV1126B
		 */
		if (vop->version >= VOP_VERSION_RK3576_LITE && vop->version <= VOP_VERSION_RK3288)
			vop_set_out_mode(vop, s->output_mode);
		else
			vop_set_out_mode(vop, ROCKCHIP_OUT_MODE_P888);
		vop_mcu_mode_setup(crtc);
	}

	dclk_inv = (s->bus_flags & DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE) ? 1 : 0;
	VOP_CTRL_SET(vop, dclk_pol, dclk_inv);
	val = (adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC) ?
		   0 : BIT(HSYNC_POSITIVE);
	val |= (adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC) ?
		   0 : BIT(VSYNC_POSITIVE);
	VOP_CTRL_SET(vop, pin_pol, val);

	if (vop->dclk_source && vop->pll && vop->pll->pll) {
		if (clk_set_parent(vop->dclk_source, vop->pll->pll))
			DRM_DEV_ERROR(vop->dev,
				      "failed to set dclk's parents\n");
	}

	switch (s->output_type) {
	case DRM_MODE_CONNECTOR_DPI:
		/*
		 * In order to ensure that the data is sampled along the rising
		 * edge without flag DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE, dclk of
		 * bt1120/bt656/rgb need to be inverted by default on rv1106 and
		 * rk3506.
		 */
		if (vop->version == VOP_VERSION_RV1106 || vop->version == VOP_VERSION_RK3506)
			dclk_inv = !dclk_inv;
		fallthrough;
	case DRM_MODE_CONNECTOR_LVDS:
		VOP_CTRL_SET(vop, rgb_en, 1);
		VOP_CTRL_SET(vop, rgb_pin_pol, val);
		VOP_CTRL_SET(vop, rgb_dclk_pol, dclk_inv);
		VOP_CTRL_SET(vop, lvds_en, 1);
		VOP_CTRL_SET(vop, lvds_pin_pol, val);
		VOP_CTRL_SET(vop, lvds_dclk_pol, dclk_inv);
		VOP_GRF_SET(vop, grf, grf_dclk_inv, dclk_inv);
		if (s->output_if & VOP_OUTPUT_IF_BT1120) {
			VOP_CTRL_SET(vop, bt1120_en, 1);
			yc_swap = is_yc_swap(s->bus_format);
			VOP_CTRL_SET(vop, bt1120_yc_swap, yc_swap);
			VOP_CTRL_SET(vop, yuv_clip, 1);
		} else if (s->output_if & VOP_OUTPUT_IF_BT656) {
			VOP_CTRL_SET(vop, bt656_en, 1);
			yc_swap = is_yc_swap(s->bus_format);
			VOP_CTRL_SET(vop, bt1120_yc_swap, yc_swap);
		}
		break;
	case DRM_MODE_CONNECTOR_eDP:
		VOP_CTRL_SET(vop, edp_en, 1);
		VOP_CTRL_SET(vop, edp_pin_pol, val);
		VOP_CTRL_SET(vop, edp_dclk_pol, dclk_inv);
		VOP_CTRL_SET(vop, inf_out_en, 1);
		VOP_CTRL_SET(vop, out_dresetn, 1);
		VOP_GRF_SET(vop, vo0_grf, grf_edp_ch_sel, 1);
		break;
	case DRM_MODE_CONNECTOR_HDMIA:
		VOP_CTRL_SET(vop, hdmi_en, 1);
		VOP_CTRL_SET(vop, hdmi_pin_pol, val);
		VOP_CTRL_SET(vop, hdmi_dclk_pol, 1);
		VOP_CTRL_SET(vop, inf_out_en, 1);
		VOP_CTRL_SET(vop, out_dresetn, 1);
		VOP_GRF_SET(vop, vo0_grf, grf_hdmi_ch_sel, 1);
		VOP_GRF_SET(vop, vo0_grf, grf_hdmi_pin_pol, val);
		VOP_GRF_SET(vop, vo0_grf, grf_hdmi_1to4_en, 1);
		break;
	case DRM_MODE_CONNECTOR_DSI:
		/*
		 * RK3576 DSI CTRL hsync/vsync polarity is positive and can't update,
		 * so set VOP hsync/vsync polarity as positive by default.
		 */
		if (vop->version == VOP_VERSION_RK3576_LITE)
			val = BIT(HSYNC_POSITIVE) | BIT(VSYNC_POSITIVE);
		VOP_CTRL_SET(vop, mipi_en, 1);
		VOP_CTRL_SET(vop, mipi_pin_pol, val);
		VOP_CTRL_SET(vop, mipi_dclk_pol, dclk_inv);
		VOP_CTRL_SET(vop, mipi_dual_channel_en,
			!!(s->output_flags & ROCKCHIP_OUTPUT_DUAL_CHANNEL_LEFT_RIGHT_MODE));
		VOP_CTRL_SET(vop, data01_swap,
			!!(s->output_flags & ROCKCHIP_OUTPUT_DATA_SWAP) ||
			vop->dual_channel_swap);
		VOP_CTRL_SET(vop, inf_out_en, 1);
		VOP_CTRL_SET(vop, out_dresetn, 1);
		VOP_GRF_SET(vop, vo0_grf, grf_mipi_ch_sel, 1);
		VOP_GRF_SET(vop, vo0_grf, grf_mipi_mode, 0);
		VOP_GRF_SET(vop, vo0_grf, grf_mipi_pin_pol, val);
		VOP_GRF_SET(vop, vo0_grf, grf_mipi_1to4_en, 1);
		if (s->hold_mode) {
			VOP_CTRL_SET(vop, edpi_te_en, !s->soft_te);
			VOP_CTRL_SET(vop, edpi_ctrl_mode, 1);
		}
		break;
	case DRM_MODE_CONNECTOR_DisplayPort:
		VOP_CTRL_SET(vop, dp_dclk_pol, 0);
		VOP_CTRL_SET(vop, dp_pin_pol, val);
		VOP_CTRL_SET(vop, dp_en, 1);
		break;
	case DRM_MODE_CONNECTOR_TV:
		if (vdisplay == CVBS_PAL_VDISPLAY)
			VOP_CTRL_SET(vop, tve_sw_mode, 1);
		else
			VOP_CTRL_SET(vop, tve_sw_mode, 0);

		VOP_CTRL_SET(vop, tve_dclk_pol, 1);
		VOP_CTRL_SET(vop, tve_dclk_en, 1);
		/* use the same pol reg with hdmi */
		VOP_CTRL_SET(vop, hdmi_pin_pol, val);
		VOP_CTRL_SET(vop, sw_genlock, 1);
		VOP_CTRL_SET(vop, sw_uv_offset_en, 1);
		VOP_CTRL_SET(vop, dither_up_en, 1);
		break;
	default:
		DRM_ERROR("unsupported connector_type[%d]\n", s->output_type);
	}
	vop_update_csc(crtc);
	VOP_CTRL_SET(vop, htotal_pw, (htotal << 16) | hsync_len);
	val = hact_st << 16;
	val |= hact_end;
	VOP_CTRL_SET(vop, hact_st_end, val);
	VOP_CTRL_SET(vop, hpost_st_end, val);

	val = vact_st << 16;
	val |= vact_end;
	VOP_CTRL_SET(vop, vact_st_end, val);
	VOP_CTRL_SET(vop, vpost_st_end, val);

	if (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		u16 vact_st_f1 = vtotal + vact_st + 1;
		u16 vact_end_f1 = vact_st_f1 + vdisplay;

		val = vact_st_f1 << 16 | vact_end_f1;
		VOP_CTRL_SET(vop, vact_st_end_f1, val);
		VOP_CTRL_SET(vop, vpost_st_end_f1, val);

		val = vtotal << 16 | (vtotal + vsync_len);
		VOP_CTRL_SET(vop, vs_st_end_f1, val);
		VOP_CTRL_SET(vop, dsp_interlace, 1);
		VOP_CTRL_SET(vop, p2i_en, 1);
		vtotal += vtotal + 1;
		act_end = vact_end_f1;
	} else {
		VOP_CTRL_SET(vop, dsp_interlace, 0);
		VOP_CTRL_SET(vop, p2i_en, 0);
		act_end = vact_end;
	}

	if (vop->version == VOP_VERSION_RK3368 || vop->version == VOP_VERSION_RK3328)
		for_ddr_freq = 1000;
	VOP_INTR_SET(vop, line_flag_num[0], act_end);
	VOP_INTR_SET(vop, line_flag_num[1],
		     act_end - us_to_vertical_line(adjusted_mode, for_ddr_freq));

	VOP_CTRL_SET(vop, vtotal_pw, vtotal << 16 | vsync_len);

	VOP_CTRL_SET(vop, core_dclk_div,
		     !!(adjusted_mode->flags & DRM_MODE_FLAG_DBLCLK) ||
		     s->output_if & VOP_OUTPUT_IF_BT656);

	VOP_CTRL_SET(vop, win_csc_mode_sel, 1);

	rockchip_drm_dclk_set_rate(vop->version, vop->dclk, adjusted_mode->crtc_clock * 1000);

	vop_cfg_done(vop);

	drm_crtc_vblank_on(crtc);
	vop_unlock(vop);
}

static int vop_zpos_cmp(const void *a, const void *b)
{
	struct vop_zpos *pa = (struct vop_zpos *)a;
	struct vop_zpos *pb = (struct vop_zpos *)b;

	return pa->zpos - pb->zpos;
}

static int vop_afbdc_atomic_check(struct drm_crtc *crtc,
				  struct drm_crtc_state *crtc_state)
{
	struct vop *vop = to_vop(crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_atomic_state *state = crtc_state->state;
	struct drm_plane *plane;
	struct drm_plane_state *pstate;
	struct vop_plane_state *plane_state;
	struct drm_framebuffer *fb;
	struct drm_rect *src;
	struct vop_win *win;
	int afbdc_format;

	s->afbdc_en = 0;

	drm_atomic_crtc_state_for_each_plane(plane, crtc_state) {
		pstate = drm_atomic_get_existing_plane_state(state, plane);
		/*
		 * plane might not have changed, in which case take
		 * current state:
		 */
		if (!pstate)
			pstate = plane->state;

		fb = pstate->fb;

		if (pstate->crtc != crtc || !fb)
			continue;

		if (!rockchip_afbc(plane, fb->modifier))
			continue;

		if (!VOP_CTRL_SUPPORT(vop, afbdc_en)) {
			DRM_INFO("not support afbdc\n");
			return -EINVAL;
		}

		plane_state = to_vop_plane_state(pstate);

		switch (plane_state->format) {
		case VOP_FMT_ARGB8888:
			afbdc_format = AFBDC_FMT_U8U8U8U8;
			break;
		case VOP_FMT_RGB888:
			afbdc_format = AFBDC_FMT_U8U8U8;
			break;
		case VOP_FMT_RGB565:
			afbdc_format = AFBDC_FMT_RGB565;
			break;
		default:
			return -EINVAL;
		}

		if (s->afbdc_en) {
			DRM_ERROR("vop only support one afbc layer\n");
			return -EINVAL;
		}

		win = to_vop_win(plane);
		src = &plane_state->src;
		if (!(win->feature & WIN_FEATURE_AFBDC)) {
			DRM_ERROR("win[%d] feature:0x%llx, not support afbdc\n",
				  win->win_id, win->feature);
			return -EINVAL;
		}
		if (!IS_ALIGNED(fb->width, 16)) {
			DRM_ERROR("win[%d] afbdc must 16 align, width: %d\n",
				  win->win_id, fb->width);
			return -EINVAL;
		}

		if (VOP_CTRL_SUPPORT(vop, afbdc_pic_vir_width)) {
			u32 align_x1, align_x2, align_y1, align_y2, align_val;
			struct drm_gem_object *obj;
			struct rockchip_gem_object *rk_obj;
			dma_addr_t fb_addr;

			obj = fb->obj[0];
			rk_obj = to_rockchip_obj(obj);
			fb_addr = rk_obj->dma_addr + fb->offsets[0];

			s->afbdc_win_format = afbdc_format;
			/* Enable color transform for YTR */
			if (fb->modifier & AFBC_FORMAT_MOD_YTR)
				s->afbdc_win_format |= AFBC_Y2R_COLOR_TRANSFORM;
			s->afbdc_win_id = win->win_id;
			s->afbdc_win_ptr = fb_addr;
			s->afbdc_win_vir_width = fb->width;
			s->afbdc_win_xoffset = (src->x1 >> 16);
			s->afbdc_win_yoffset = (src->y1 >> 16);

			align_x1 = (src->x1 >> 16) - ((src->x1 >> 16) % 16);
			align_y1 = (src->y1 >> 16) - ((src->y1 >> 16) % 16);

			align_val = (src->x2 >> 16) % 16;
			if (align_val)
				align_x2 = (src->x2 >> 16) + (16 - align_val);
			else
				align_x2 = src->x2 >> 16;

			align_val = (src->y2 >> 16) % 16;
			if (align_val)
				align_y2 = (src->y2 >> 16) + (16 - align_val);
			else
				align_y2 = src->y2 >> 16;

			s->afbdc_win_width = align_x2 - align_x1 - 1;
			s->afbdc_win_height = align_y2 - align_y1 - 1;

			s->afbdc_en = 1;

			break;
		}
		if (src->x1 || src->y1 || fb->offsets[0]) {
			DRM_ERROR("win[%d] afbdc not support offset display\n",
				  win->win_id);
			DRM_ERROR("xpos=%d, ypos=%d, offset=%d\n",
				  src->x1, src->y1, fb->offsets[0]);
			return -EINVAL;
		}
		s->afbdc_win_format = afbdc_format;
		/* Enable color transform for YTR */
		if (fb->modifier & AFBC_FORMAT_MOD_YTR)
			s->afbdc_win_format |= AFBC_Y2R_COLOR_TRANSFORM;
		s->afbdc_win_width = fb->width - 1;
		/* RK3399 AFBC can't supported vir width */
		if (vop->version == VOP_VERSION_RK3399_BIG &&
		    fb->width != drm_rect_width(src) >> 16) {
			DRM_ERROR("Win%d afbc unsupported vir width: fb->width: %d, act_w: %d\n",
				  win->win_id, fb->width, drm_rect_width(src) >> 16);
			return -EINVAL;
		}
		s->afbdc_win_height = (drm_rect_height(src) >> 16) - 1;
		s->afbdc_win_id = win->win_id;
		s->afbdc_win_ptr = plane_state->yrgb_mst;
		s->afbdc_en = 1;
	}

	return 0;
}

static void vop_dclk_source_generate(struct drm_crtc *crtc,
				     struct drm_crtc_state *crtc_state)
{
	struct rockchip_drm_private *private = crtc->dev->dev_private;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct rockchip_crtc_state *old_s = to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);
	struct rockchip_dclk_pll *old_pll = vop->pll;

	if (!vop->dclk_source)
		return;

	if (crtc_state->active) {
		WARN_ON(vop->pll && !vop->pll->use_count);
		if (!vop->pll || vop->pll->use_count > 1 ||
		    s->output_type != old_s->output_type) {
			if (vop->pll)
				vop->pll->use_count--;

			if (s->output_type != DRM_MODE_CONNECTOR_HDMIA &&
			    !private->default_pll.use_count)
				vop->pll = &private->default_pll;
			else
				vop->pll = &private->hdmi_pll;

			vop->pll->use_count++;
		}
	} else if (vop->pll) {
		vop->pll->use_count--;
		vop->pll = NULL;
	}
	if (vop->pll != old_pll)
		crtc_state->mode_changed = true;
}

static int vop_crtc_atomic_check(struct drm_crtc *crtc,
				 struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state,
									  crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;
	struct drm_plane *plane;
	struct drm_plane_state *pstate;
	struct vop_plane_state *plane_state;
	struct vop_zpos *pzpos, *zpos;
	const struct vop_win_data *win_data;
	int dsp_layer_sel = 0;
	int i, j, cnt = 0, ret = 0, shift;

	ret = vop_afbdc_atomic_check(crtc, crtc_state);
	if (ret)
		return ret;

	s->yuv_overlay = 0;
	if (VOP_CTRL_SUPPORT(vop, overlay_mode))
		s->yuv_overlay = is_yuv_output(s->bus_format);

	ret = vop_hdr_atomic_check(crtc, crtc_state);
	if (ret)
		return ret;
	ret = vop_csc_atomic_check(crtc, crtc_state);
	if (ret)
		return ret;

	pzpos = kmalloc_array(vop_data->win_size, sizeof(*pzpos), GFP_KERNEL);
	if (!pzpos)
		return -ENOMEM;

	for (i = 0; i < vop_data->win_size; i++) {
		const struct vop_win_data *win_data = &vop_data->win[i];
		struct vop_win *win;

		if (!win_data->phy)
			continue;

		for (j = 0; j < vop->num_wins; j++) {
			win = &vop->win[j];

			if (win->win_id == i && !win->area_id)
				break;
		}
		if (WARN_ON(j >= vop->num_wins)) {
			ret = -EINVAL;
			goto err_free_pzpos;
		}

		plane = &win->base;
		pstate = state->planes[drm_plane_index(plane)].state;
		/*
		 * plane might not have changed, in which case take
		 * current state:
		 */
		if (!pstate)
			pstate = plane->state;
		plane_state = to_vop_plane_state(pstate);

		if (!pstate->visible)
			pzpos[cnt].zpos = INT_MAX;
		else
			pzpos[cnt].zpos = plane_state->zpos;
		pzpos[cnt++].win_id = win->win_id;
	}

	sort(pzpos, cnt, sizeof(pzpos[0]), vop_zpos_cmp, NULL);

	for (i = 0, cnt = 0; i < vop_data->win_size; i++) {
		win_data = &vop_data->win[i];
		shift = i * 2;
		zpos = &pzpos[cnt];

		/*
		 * 1. For RV1126B:
		 * dsp_layer2_sel, that is top layer:
		 * - 2'b00 WIN0 is top layer
		 * - 2'b01 WIN2 is top layer
		 * dsp_layer1_sel, that is bottom layer:
		 * - 2'b00 WIN0 is bottom layer
		 * - 2'b01 WIN2 is bottom layer
		 *
		 * For {dsp_layer2_sel, dsp_layer1_sel}:
		 * - 4'b0001 : WIN0 on the top, WIN2 on the bottom
		 * - 4'b0100 : WIN2 on the top, WIN0 on the bottom
		 *
		 * 2. For other version:
		 * - 2'b00 select WIN0
		 * - 2'b10 select WIN2
		 */
		if (vop->version == VOP_VERSION_RV1126B) {
			if (win_data->phy) {
				/* only set top layer */
				if (cnt++)
					dsp_layer_sel |= BIT(zpos->win_id);
			}
		} else {
			if (win_data->phy) {
				cnt++;
				dsp_layer_sel |= zpos->win_id << shift;
			} else {
				dsp_layer_sel |= i << shift;
			}
		}
	}

	s->dsp_layer_sel = dsp_layer_sel;

	vop_dclk_source_generate(crtc, crtc_state);

err_free_pzpos:
	kfree(pzpos);
	return ret;
}

static void vop_post_config(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	u16 vtotal = mode->crtc_vtotal;
	u16 hdisplay = mode->crtc_hdisplay;
	u16 hact_st = mode->crtc_htotal - mode->crtc_hsync_start;
	u16 vdisplay = mode->crtc_vdisplay;
	u16 vact_st = mode->crtc_vtotal - mode->crtc_vsync_start;
	u16 hsize = hdisplay * (s->left_margin + s->right_margin) / 200;
	u16 vsize = vdisplay * (s->top_margin + s->bottom_margin) / 200;
	u16 hact_end, vact_end;
	u32 val;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		vsize = rounddown(vsize, 2);

	hact_st += hdisplay * (100 - s->left_margin) / 200;
	hact_end = hact_st + hsize;
	val = hact_st << 16;
	val |= hact_end;
	VOP_CTRL_SET(vop, hpost_st_end, val);
	vact_st += vdisplay * (100 - s->top_margin) / 200;
	vact_end = vact_st + vsize;
	val = vact_st << 16;
	val |= vact_end;
	VOP_CTRL_SET(vop, vpost_st_end, val);
	val = scl_cal_scale2(vdisplay, vsize) << 16;
	val |= scl_cal_scale2(hdisplay, hsize);
	VOP_CTRL_SET(vop, post_scl_factor, val);

#define POST_HORIZONTAL_SCALEDOWN_EN(x)		((x) << 0)
#define POST_VERTICAL_SCALEDOWN_EN(x)		((x) << 1)
	VOP_CTRL_SET(vop, post_scl_ctrl,
		     POST_HORIZONTAL_SCALEDOWN_EN(hdisplay != hsize) |
		     POST_VERTICAL_SCALEDOWN_EN(vdisplay != vsize));
	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		u16 vact_st_f1 = vtotal + vact_st + 1;
		u16 vact_end_f1 = vact_st_f1 + vsize;

		val = vact_st_f1 << 16 | vact_end_f1;
		VOP_CTRL_SET(vop, vpost_st_end_f1, val);
	}
}

static void vop_update_hdr(struct drm_crtc *crtc,
			   struct drm_crtc_state *old_crtc_state)
{
	struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);
	struct rockchip_sdr2hdr_state *sdr2hdr_state = &s->hdr.sdr2hdr_state;

	if (!vop->data->hdr_table)
		return;

	if (s->hdr.hdr2sdr_en) {
		vop_load_hdr2sdr_table(vop);
		/* This is ic design bug, when in hdr2sdr mode, the overlay mode
		 * is rgb domain, so the win0 is do yuv2rgb, but in this case,
		 * we must close win0 y2r.
		 */
		VOP_CTRL_SET(vop, hdr2sdr_en_win0_csc, 0);
	}
	VOP_CTRL_SET(vop, hdr2sdr_en, s->hdr.hdr2sdr_en);

	VOP_CTRL_SET(vop, bt1886eotf_pre_conv_en,
		     sdr2hdr_state->bt1886eotf_pre_conv_en);
	VOP_CTRL_SET(vop, bt1886eotf_post_conv_en,
		     sdr2hdr_state->bt1886eotf_post_conv_en);

	VOP_CTRL_SET(vop, rgb2rgb_pre_conv_en,
		     sdr2hdr_state->rgb2rgb_pre_conv_en);
	VOP_CTRL_SET(vop, rgb2rgb_pre_conv_mode,
		     sdr2hdr_state->rgb2rgb_pre_conv_mode);
	VOP_CTRL_SET(vop, st2084oetf_pre_conv_en,
		     sdr2hdr_state->st2084oetf_pre_conv_en);

	VOP_CTRL_SET(vop, rgb2rgb_post_conv_en,
		     sdr2hdr_state->rgb2rgb_post_conv_en);
	VOP_CTRL_SET(vop, rgb2rgb_post_conv_mode,
		     sdr2hdr_state->rgb2rgb_post_conv_mode);
	VOP_CTRL_SET(vop, st2084oetf_post_conv_en,
		     sdr2hdr_state->st2084oetf_post_conv_en);

	if (sdr2hdr_state->bt1886eotf_pre_conv_en ||
	    sdr2hdr_state->bt1886eotf_post_conv_en)
		vop_load_sdr2hdr_table(vop, sdr2hdr_state->sdr2hdr_func);
	VOP_CTRL_SET(vop, win_csc_mode_sel, 1);
}

static void vop_tv_config_update(struct drm_crtc *crtc,
				 struct drm_crtc_state *old_crtc_state)
{
	struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
	struct rockchip_crtc_state *old_s =
			to_rockchip_crtc_state(old_crtc_state);
	int brightness, contrast, saturation, hue, sin_hue, cos_hue;
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;

	if (!s->tv_state)
		return;

	/*
	 * The BCSH only need to config once except one of the following
	 * condition changed:
	 *   1. tv_state: include brightness,contrast,saturation and hue;
	 *   2. yuv_overlay: it is related to BCSH r2y module;
	 *   3. mode_update: it is indicate mode change and resume from suspend;
	 *   4. bcsh_en: control the BCSH module enable or disable state;
	 *   5. bus_format: it is related to BCSH y2r module;
	 */
	if (!memcmp(s->tv_state,
		    &vop->active_tv_state, sizeof(*s->tv_state)) &&
	    s->yuv_overlay == old_s->yuv_overlay && s->mode_update &&
	    s->bcsh_en == old_s->bcsh_en && s->bus_format == old_s->bus_format)
		return;

	memcpy(&vop->active_tv_state, s->tv_state, sizeof(*s->tv_state));
	/* post BCSH CSC */
	s->post_r2y_en = 0;
	s->post_y2r_en = 0;
	s->bcsh_en = 0;
	if (s->tv_state) {
		if (s->tv_state->brightness != 50 ||
		    s->tv_state->contrast != 50 ||
		    s->tv_state->saturation != 50 || s->tv_state->hue != 50)
			s->bcsh_en = 1;
	}

	if (s->bcsh_en) {
		if (!s->yuv_overlay)
			s->post_r2y_en = 1;
		if (!is_yuv_output(s->bus_format))
			s->post_y2r_en = 1;
	} else {
		if (!s->yuv_overlay && is_yuv_output(s->bus_format))
			s->post_r2y_en = 1;
		if (s->yuv_overlay && !is_yuv_output(s->bus_format))
			s->post_y2r_en = 1;
	}

	s->post_csc_mode = to_vop_csc_mode(s->color_encoding, s->color_range);
	VOP_CTRL_SET(vop, bcsh_r2y_en, s->post_r2y_en);
	VOP_CTRL_SET(vop, bcsh_y2r_en, s->post_y2r_en);
	VOP_CTRL_SET(vop, bcsh_r2y_csc_mode, s->post_csc_mode);
	VOP_CTRL_SET(vop, bcsh_y2r_csc_mode, s->post_csc_mode);
	if (!s->bcsh_en) {
		VOP_CTRL_SET(vop, bcsh_en, s->bcsh_en);
		return;
	}

	if (vop_data->feature & VOP_FEATURE_OUTPUT_10BIT)
		brightness = interpolate(0, -128, 100, 127, s->tv_state->brightness);
	else if (vop->version == VOP_VERSION_PX30_BIG || vop->version >= VOP_VERSION_RK3506)
		brightness = interpolate(0, -64, 100, 63, s->tv_state->brightness);
	else
		brightness = interpolate(0, -32, 100, 31, s->tv_state->brightness);

	if ((VOP_MAJOR(vop->version) == 3) ||
	    (VOP_MAJOR(vop->version) == 2 && VOP_MINOR(vop->version) == 6)) { /* px30 vopb */
		contrast = interpolate(0, 0, 100, 511, s->tv_state->contrast);
		saturation = interpolate(0, 0, 100, 511, s->tv_state->saturation);
		/*
		 *  a:[-30~0]:
		 *    sin_hue = 0x100 - sin(a)*256;
		 *    cos_hue = cos(a)*256;
		 *  a:[0~30]
		 *    sin_hue = sin(a)*256;
		 *    cos_hue = cos(a)*256;
		 */
		hue = interpolate(0, -30, 100, 30, s->tv_state->hue);
		sin_hue = fixp_sin32(hue) >> 23;
		cos_hue = fixp_cos32(hue) >> 23;
		VOP_CTRL_SET(vop, bcsh_sat_con, saturation * contrast / 0x100);

	} else {
		contrast = interpolate(0, 0, 100, 255, s->tv_state->contrast);
		saturation = interpolate(0, 0, 100, 255, s->tv_state->saturation);
		/*
		 *  a:[-30~0]:
		 *    sin_hue = 0x100 - sin(a)*128;
		 *    cos_hue = cos(a)*128;
		 *  a:[0~30]
		 *    sin_hue = sin(a)*128;
		 *    cos_hue = cos(a)*128;
		 */
		hue = interpolate(0, -30, 100, 30, s->tv_state->hue);
		sin_hue = fixp_sin32(hue) >> 24;
		cos_hue = fixp_cos32(hue) >> 24;
		VOP_CTRL_SET(vop, bcsh_sat_con, saturation * contrast / 0x80);
	}

	VOP_CTRL_SET(vop, bcsh_brightness, brightness);
	VOP_CTRL_SET(vop, bcsh_contrast, contrast);
	VOP_CTRL_SET(vop, bcsh_sin_hue, sin_hue);
	VOP_CTRL_SET(vop, bcsh_cos_hue, cos_hue);
	VOP_CTRL_SET(vop, bcsh_out_mode, BCSH_OUT_MODE_NORMAL_VIDEO);
	if (vop->version == VOP_VERSION_RK3288)
		VOP_CTRL_SET(vop, auto_gate_en, 0);
	VOP_CTRL_SET(vop, bcsh_en, s->bcsh_en);
}

static void vop_cfg_update(struct drm_crtc *crtc,
			   struct drm_crtc_state *old_crtc_state)
{
	struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;

	spin_lock(&vop->reg_lock);

	vop_update_csc(crtc);

	vop_tv_config_update(crtc, old_crtc_state);

	if (s->afbdc_en) {
		u32 pic_size, pic_offset;

		VOP_CTRL_SET(vop, afbdc_format, s->afbdc_win_format);
		VOP_CTRL_SET(vop, afbdc_hreg_block_split, 0);
		VOP_CTRL_SET(vop, afbdc_sel, s->afbdc_win_id);
		VOP_CTRL_SET(vop, afbdc_hdr_ptr, s->afbdc_win_ptr);
		pic_size = (s->afbdc_win_width & 0xffff);
		pic_size |= s->afbdc_win_height << 16;
		VOP_CTRL_SET(vop, afbdc_pic_size, pic_size);

		VOP_CTRL_SET(vop, afbdc_pic_vir_width, s->afbdc_win_vir_width);
		pic_offset = (s->afbdc_win_xoffset & 0xffff);
		pic_offset |= s->afbdc_win_yoffset << 16;
		VOP_CTRL_SET(vop, afbdc_pic_offset, pic_offset);
	}

	VOP_CTRL_SET(vop, afbdc_en, s->afbdc_en);

	VOP_CTRL_SET(vop, dsp_layer_sel, s->dsp_layer_sel);
	if (vop_data->feature & VOP_FEATURE_OVERSCAN)
		vop_post_config(crtc);

	spin_unlock(&vop->reg_lock);
}

static bool vop_fs_irq_is_pending(struct vop *vop)
{
	if (vop->version == VOP_VERSION_RK3228 || vop->version == VOP_VERSION_RK3328)
		return VOP_INTR_GET_TYPE(vop, status, FS_FIELD_INTR);
	else
		return VOP_INTR_GET_TYPE(vop, status, FS_INTR);
}

static void vop_wait_for_irq_handler(struct vop *vop)
{
	bool pending;
	int ret;

	/*
	 * Spin until frame start interrupt status bit goes low, which means
	 * that interrupt handler was invoked and cleared it. The timeout of
	 * 10 msecs is really too long, but it is just a safety measure if
	 * something goes really wrong. The wait will only happen in the very
	 * unlikely case of a vblank happening exactly at the same time and
	 * shouldn't exceed microseconds range.
	 */
	ret = readx_poll_timeout_atomic(vop_fs_irq_is_pending, vop, pending,
					!pending, 0, 10 * 1000);
	if (ret)
		DRM_DEV_ERROR(vop->dev, "VOP vblank IRQ stuck for 10 ms\n");

	synchronize_irq(vop->irq);
}

static void vop_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_crtc_state = drm_atomic_get_old_crtc_state(state,
									      crtc);
	struct drm_atomic_state *old_state = old_crtc_state->state;
	struct drm_plane_state *old_plane_state;
	struct vop *vop = to_vop(crtc);
	struct drm_plane *plane;
	int i;
	unsigned long flags;
	struct rockchip_crtc_state *s =
		to_rockchip_crtc_state(crtc->state);

#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
	if (vop->rockchip_crtc.vop_dump_status == DUMP_KEEP ||
	    vop->rockchip_crtc.vop_dump_times > 0) {
		rockchip_drm_crtc_dump_plane_buffer(crtc);
		vop->rockchip_crtc.vop_dump_times--;
	}
#endif

	vop_cfg_update(crtc, old_crtc_state);

	if (!vop->is_iommu_enabled) {
		int ret;

		if (s->mode_update)
			VOP_CTRL_SET(vop, dma_stop, 1);

		ret = rockchip_drm_dma_attach_device(vop->drm_dev, vop->dev);
		if (ret) {
			vop->is_iommu_enabled = false;
			vop_disable_all_planes(vop);
			dev_err(vop->dev, "failed to attach dma mapping, %d\n",
				ret);
		} else {
			vop->is_iommu_enabled = true;
			VOP_CTRL_SET(vop, dma_stop, 0);
		}
	}

	vop_update_hdr(crtc, old_crtc_state);
	if (old_crtc_state->color_mgmt_changed || old_crtc_state->active_changed) {
		if (crtc->state->gamma_lut || vop->gamma_lut) {
			if (old_crtc_state->gamma_lut)
				vop->gamma_lut = old_crtc_state->gamma_lut->data;
			vop_crtc_atomic_gamma_set(crtc, old_crtc_state);
		}
	}

	spin_lock_irqsave(&vop->irq_lock, flags);
	vop->pre_overlay = s->hdr.pre_overlay;
	vop_wb_commit(crtc);
	vop_cfg_done(vop);
	rockchip_drm_dbg(vop->dev, VOP_DEBUG_CFG_DONE, "cfg_done\n");
	/*
	 * rk322x and rk332x odd-even field will mistake when in interlace mode.
	 * we must switch to frame effect before switch screen and switch to
	 * field effect after switch screen complete.
	 */
	if (VOP_MAJOR(vop->version) == 3 &&
	    (VOP_MINOR(vop->version) == 7 || VOP_MINOR(vop->version) == 8)) {
		if (!s->mode_update && VOP_CTRL_GET(vop, reg_done_frm))
			VOP_CTRL_SET(vop, reg_done_frm, 0);
	} else {
		VOP_CTRL_SET(vop, reg_done_frm, 0);
	}
	if (vop->mcu_timing.mcu_pix_total && !s->soft_te)
		VOP_CTRL_SET(vop, mcu_hold_mode, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	/*
	 * There is a (rather unlikely) possiblity that a vblank interrupt
	 * fired before we set the cfg_done bit. To avoid spuriously
	 * signalling flip completion we need to wait for it to finish.
	 */
	vop_wait_for_irq_handler(vop);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (crtc->state->event) {
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		WARN_ON(vop->event);

		vop->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		if (!old_plane_state->fb)
			continue;

		if (old_plane_state->fb == plane->state->fb)
			continue;

		drm_framebuffer_get(old_plane_state->fb);
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		drm_flip_work_queue(&vop->fb_unref_work, old_plane_state->fb);
		set_bit(VOP_PENDING_FB_UNREF, &vop->pending);
	}
}

static const struct drm_crtc_helper_funcs vop_crtc_helper_funcs = {
	.mode_valid = vop_crtc_mode_valid,
	.mode_fixup = vop_crtc_mode_fixup,
	.atomic_check = vop_crtc_atomic_check,
	.atomic_flush = vop_crtc_atomic_flush,
	.atomic_enable = vop_crtc_atomic_enable,
	.atomic_disable = vop_crtc_atomic_disable,
};

static void vop_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void vop_crtc_reset(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);
		kfree(s);
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return;
	crtc->state = &s->base;
	crtc->state->crtc = crtc;

	s->left_margin = 100;
	s->right_margin = 100;
	s->top_margin = 100;
	s->bottom_margin = 100;
}

static struct drm_crtc_state *vop_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *rockchip_state;

	if (WARN_ON(!crtc->state))
		return NULL;

	rockchip_state = kmemdup(to_rockchip_crtc_state(crtc->state),
				 sizeof(*rockchip_state), GFP_KERNEL);
	if (!rockchip_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &rockchip_state->base);
	return &rockchip_state->base;
}

static void vop_crtc_destroy_state(struct drm_crtc *crtc,
				   struct drm_crtc_state *state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(&s->base);
	kfree(s);
}

#ifdef CONFIG_DRM_ANALOGIX_DP
static struct drm_connector *vop_get_edp_connector(struct vop *vop)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(vop->drm_dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->connector_type == DRM_MODE_CONNECTOR_eDP) {
			drm_connector_list_iter_end(&conn_iter);
			return connector;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return NULL;
}

static int vop_crtc_set_crc_source(struct drm_crtc *crtc,
				   const char *source_name)
{
	struct vop *vop = to_vop(crtc);
	struct drm_connector *connector;
	int ret;

	connector = vop_get_edp_connector(vop);
	if (!connector)
		return -EINVAL;

	if (source_name && strcmp(source_name, "auto") == 0)
		ret = analogix_dp_start_crc(connector);
	else if (!source_name)
		ret = analogix_dp_stop_crc(connector);
	else
		ret = -EINVAL;

	return ret;
}

static int
vop_crtc_verify_crc_source(struct drm_crtc *crtc, const char *source_name,
			   size_t *values_cnt)
{
	if (source_name && strcmp(source_name, "auto") != 0)
		return -EINVAL;

	*values_cnt = 3;
	return 0;
}

#else
static int vop_crtc_set_crc_source(struct drm_crtc *crtc,
				   const char *source_name)
{
	return -ENODEV;
}

static int
vop_crtc_verify_crc_source(struct drm_crtc *crtc, const char *source_name,
			   size_t *values_cnt)
{
	return -ENODEV;
}
#endif

static int vop_crtc_atomic_get_property(struct drm_crtc *crtc,
					const struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct drm_device *drm_dev = crtc->dev;
	struct rockchip_drm_private *private = drm_dev->dev_private;
	struct drm_mode_config *mode_config = &drm_dev->mode_config;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(state);
	struct vop *vop = to_vop(crtc);

	if (property == mode_config->tv_left_margin_property) {
		*val = s->left_margin;
		return 0;
	}

	if (property == mode_config->tv_right_margin_property) {
		*val = s->right_margin;
		return 0;
	}

	if (property == mode_config->tv_top_margin_property) {
		*val = s->top_margin;
		return 0;
	}

	if (property == mode_config->tv_bottom_margin_property) {
		*val = s->bottom_margin;
		return 0;
	}

	if (property == private->aclk_prop) {
		/* KHZ, keep align with mode->clock */
		*val = clk_get_rate(vop->aclk) / 1000;
		return 0;
	}

	if (property == private->bg_prop) {
		*val = vop->background;
		return 0;
	}

	if (property == private->line_flag_prop) {
		*val = vop->line_flag;
		return 0;
	}

	DRM_ERROR("failed to get vop crtc property\n");
	return -EINVAL;
}

static int vop_crtc_atomic_set_property(struct drm_crtc *crtc,
					struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t val)
{
	struct drm_device *drm_dev = crtc->dev;
	struct rockchip_drm_private *private = drm_dev->dev_private;
	struct drm_mode_config *mode_config = &drm_dev->mode_config;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(state);
	struct vop *vop = to_vop(crtc);

	if (property == mode_config->tv_left_margin_property) {
		s->left_margin = val;
		return 0;
	}

	if (property == mode_config->tv_right_margin_property) {
		s->right_margin = val;
		return 0;
	}

	if (property == mode_config->tv_top_margin_property) {
		s->top_margin = val;
		return 0;
	}

	if (property == mode_config->tv_bottom_margin_property) {
		s->bottom_margin = val;
		return 0;
	}

	if (property == private->bg_prop) {
		vop->background = val;
		return 0;
	}

	if (property == private->line_flag_prop) {
		vop->line_flag = val;
		return 0;
	}

	DRM_ERROR("failed to set vop crtc property\n");
	return -EINVAL;
}

static const struct drm_crtc_funcs vop_crtc_funcs = {
	.gamma_set = vop_crtc_legacy_gamma_set,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = vop_crtc_destroy,
	.reset = vop_crtc_reset,
	.atomic_get_property = vop_crtc_atomic_get_property,
	.atomic_set_property = vop_crtc_atomic_set_property,
	.atomic_duplicate_state = vop_crtc_duplicate_state,
	.atomic_destroy_state = vop_crtc_destroy_state,
	.enable_vblank = vop_crtc_enable_vblank,
	.disable_vblank = vop_crtc_disable_vblank,
	.set_crc_source = vop_crtc_set_crc_source,
	.verify_crc_source = vop_crtc_verify_crc_source,
	.late_register = vop_crtc_late_register,
	.early_unregister = vop_crtc_early_unregister,
};

static void vop_fb_unref_worker(struct drm_flip_work *work, void *val)
{
	struct vop *vop = container_of(work, struct vop, fb_unref_work);
	struct drm_framebuffer *fb = val;

	drm_crtc_vblank_put(&vop->rockchip_crtc.crtc);
	drm_framebuffer_put(fb);
}

static void vop_handle_vblank(struct vop *vop)
{
	struct drm_device *drm = vop->drm_dev;
	struct drm_crtc *crtc = &vop->rockchip_crtc.crtc;
	unsigned long flags;

	spin_lock_irqsave(&drm->event_lock, flags);
	if (vop->event) {
		drm_crtc_send_vblank_event(crtc, vop->event);
		drm_crtc_vblank_put(crtc);
		vop->event = NULL;
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);

	if (test_and_clear_bit(VOP_PENDING_FB_UNREF, &vop->pending))
		drm_flip_work_commit(&vop->fb_unref_work, system_unbound_wq);
}

static void vop_wb_complete(struct vop *vop)
{
	struct vop_wb *wb = &vop->wb;
	bool wb_oneframe_mode;
	bool wb_en;

	wb_en = VOP_CTRL_GET2(vop, wb, enable);
	wb_oneframe_mode = VOP_CTRL_GET2(vop, wb, one_frame_mode);
	/*
	 * The write back should work in one shot mode,
	 * stop when write back complete in next vsync.
	 */
	if (wb_en && !wb_oneframe_mode)
		vop_wb_disable(vop);

	drm_writeback_signal_completion(&vop->wb.conn, 0);
}

static irqreturn_t vop_isr(int irq, void *data)
{
	struct vop *vop = data;
	struct drm_crtc *crtc = &vop->rockchip_crtc.crtc;
	uint32_t active_irqs;
	uint32_t wb_irqs;
	unsigned long flags;
	int ret = IRQ_NONE;

	/*
	 * The irq is shared with the iommu. If the runtime-pm state of the
	 * vop-device is disabled the irq has to be targeted at the iommu.
	 */
	if (!pm_runtime_get_if_in_use(vop->dev))
		return IRQ_NONE;

	if (vop_core_clks_enable(vop)) {
		DRM_DEV_ERROR_RATELIMITED(vop->dev, "couldn't enable clocks\n");
		goto out;
	}

	/*
	 * interrupt register has interrupt status, enable and clear bits, we
	 * must hold irq_lock to avoid a race with enable/disable_vblank().
	*/
	spin_lock_irqsave(&vop->irq_lock, flags);

	active_irqs = VOP_INTR_GET_TYPE(vop, status, INTR_MASK);
	/* Clear all active interrupt sources */
	if (active_irqs)
		VOP_INTR_SET_TYPE(vop, clear, active_irqs, 1);
	wb_irqs = vop_read_and_clear_wb_irqs(vop);

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	/* This is expected for vop iommu irqs, since the irq is shared */
	if (!active_irqs && !wb_irqs)
		goto out_disable;

	if (active_irqs & DSP_HOLD_VALID_INTR) {
		complete(&vop->dsp_hold_completion);
		active_irqs &= ~DSP_HOLD_VALID_INTR;
		ret = IRQ_HANDLED;
	}

	if (active_irqs & LINE_FLAG_INTR) {
		complete(&vop->line_flag_completion);
		active_irqs &= ~LINE_FLAG_INTR;
		ret = IRQ_HANDLED;
	}

	if ((active_irqs & FS_INTR) || (active_irqs & FS_FIELD_INTR)) {
		/* This is IC design not reasonable, this two register bit need
		 * frame effective, but actually it's effective immediately, so
		 * we config this register at frame start.
		 */
		rockchip_drm_dbg(vop->dev, VOP_DEBUG_VSYNC, "vsync");
		spin_lock_irqsave(&vop->irq_lock, flags);
		VOP_CTRL_SET(vop, level2_overlay_en, vop->pre_overlay);
		VOP_CTRL_SET(vop, alpha_hard_calc, vop->pre_overlay);
		spin_unlock_irqrestore(&vop->irq_lock, flags);
		drm_crtc_handle_vblank(crtc);
		vop_handle_vblank(vop);
		active_irqs &= ~(FS_INTR | FS_FIELD_INTR);
		ret = IRQ_HANDLED;
	}

#define ERROR_HANDLER(x) \
	do { \
		if (active_irqs & x##_INTR) {\
			DRM_DEV_ERROR_RATELIMITED(vop->dev, #x " irq err\n"); \
			active_irqs &= ~x##_INTR; \
			ret = IRQ_HANDLED; \
		} \
	} while (0)

	ERROR_HANDLER(BUS_ERROR);
	ERROR_HANDLER(WIN0_EMPTY);
	ERROR_HANDLER(WIN1_EMPTY);
	ERROR_HANDLER(WIN2_EMPTY);
	ERROR_HANDLER(WIN3_EMPTY);
	ERROR_HANDLER(HWC_EMPTY);
	ERROR_HANDLER(POST_BUF_EMPTY);

	/* Unhandled irqs are spurious. */
	if (active_irqs)
		DRM_ERROR("Unknown VOP IRQs: %#02x\n", active_irqs);

	if (wb_irqs) {
		active_irqs = wb_irqs;
		ERROR_HANDLER(VOPL_WB_UV_FIFO_FULL);
		ERROR_HANDLER(VOPL_WB_YRGB_FIFO_FULL);
		if (active_irqs & VOPL_WB_COMPLETE_INTR) {
			active_irqs &= ~VOPL_WB_COMPLETE_INTR;
			vop_wb_complete(vop);
		}
		if (active_irqs)
			DRM_ERROR("Unknown writeback IRQs: %02x\n", active_irqs);
	}

out_disable:
	vop_core_clks_disable(vop);
out:
	pm_runtime_put(vop->dev);
	return ret;
}

static void vop_plane_add_properties(struct vop *vop,
				     struct drm_plane *plane,
				     const struct vop_win *win)
{
	unsigned int flags = 0;

	flags |= (VOP_WIN_SUPPORT(vop, win, xmirror)) ? DRM_MODE_REFLECT_X : 0;
	flags |= (VOP_WIN_SUPPORT(vop, win, ymirror)) ? DRM_MODE_REFLECT_Y : 0;

	if (flags)
		drm_plane_create_rotation_property(plane, DRM_MODE_ROTATE_0,
						   DRM_MODE_ROTATE_0 | flags);
}

static int vop_plane_create_name_property(struct vop *vop, struct vop_win *win)
{
	struct drm_prop_enum_list *props = vop->plane_name_list;
	struct drm_property *prop;
	uint64_t bits = BIT_ULL(win->plane_id);

	prop = drm_property_create_bitmask(vop->drm_dev,
					   DRM_MODE_PROP_IMMUTABLE, "NAME",
					   props, vop->num_wins, bits);
	if (!prop) {
		DRM_DEV_ERROR(vop->dev, "create Name prop for %s failed\n", win->name);
		return -ENOMEM;
	}
	win->name_prop = prop;
	drm_object_attach_property(&win->base.base, win->name_prop, bits);

	return 0;
}

static int vop_plane_init(struct vop *vop, struct vop_win *win,
			  unsigned long possible_crtcs)
{
	struct rockchip_drm_private *private = vop->drm_dev->dev_private;
	unsigned int blend_caps = BIT(DRM_MODE_BLEND_PIXEL_NONE) | BIT(DRM_MODE_BLEND_PREMULTI) |
				  BIT(DRM_MODE_BLEND_COVERAGE);
	const struct vop_data *vop_data = vop->data;
	uint64_t feature = 0;
	int ret;

	ret = drm_universal_plane_init(vop->drm_dev, &win->base, possible_crtcs, &vop_plane_funcs,
				       win->data_formats, win->nformats, win->format_modifiers,
				       win->type, win->name);
	if (ret) {
		DRM_ERROR("failed to initialize plane %d\n", ret);
		return ret;
	}
	drm_plane_helper_add(&win->base, &plane_helper_funcs);

	if (win->phy->scl)
		feature |= BIT(ROCKCHIP_DRM_PLANE_FEATURE_SCALE);
	if (VOP_WIN_SUPPORT(vop, win, src_alpha_ctl) ||
	    VOP_WIN_SUPPORT(vop, win, alpha_en))
		feature |= BIT(ROCKCHIP_DRM_PLANE_FEATURE_ALPHA);
	if (win->feature & WIN_FEATURE_HDR2SDR)
		feature |= BIT(ROCKCHIP_DRM_PLANE_FEATURE_HDR2SDR);
	if (win->feature & WIN_FEATURE_SDR2HDR)
		feature |= BIT(ROCKCHIP_DRM_PLANE_FEATURE_SDR2HDR);
	if (win->feature & WIN_FEATURE_AFBDC)
		feature |= BIT(ROCKCHIP_DRM_PLANE_FEATURE_AFBDC);

	drm_object_attach_property(&win->base.base, vop->plane_feature_prop,
				   feature);
	drm_object_attach_property(&win->base.base, private->eotf_prop, 0);
	drm_plane_create_color_properties(&win->base,
					  BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
					  BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
					  BIT(DRM_COLOR_YCBCR_FULL_RANGE),
					  DRM_COLOR_YCBCR_BT601,
					  DRM_COLOR_YCBCR_LIMITED_RANGE);
	if (VOP_WIN_SUPPORT(vop, win, global_alpha_val))
		drm_plane_create_alpha_property(&win->base);
	drm_object_attach_property(&win->base.base,
				   private->async_commit_prop, 0);

	if (win->parent)
		drm_object_attach_property(&win->base.base, private->share_id_prop,
					   win->parent->base.base.id);
	else
		drm_object_attach_property(&win->base.base, private->share_id_prop,
					   win->base.base.id);

	drm_plane_create_blend_mode_property(&win->base, blend_caps);
	drm_plane_create_zpos_property(&win->base, win->win_id, 0, vop->num_wins - 1);
	vop_plane_create_name_property(vop, win);


	win->input_width_prop = drm_property_create_range(vop->drm_dev, DRM_MODE_PROP_IMMUTABLE,
							  "INPUT_WIDTH", 0, vop_data->max_input.width);
	win->input_height_prop = drm_property_create_range(vop->drm_dev, DRM_MODE_PROP_IMMUTABLE,
							   "INPUT_HEIGHT", 0, vop_data->max_input.height);

	win->output_width_prop = drm_property_create_range(vop->drm_dev, DRM_MODE_PROP_IMMUTABLE,
							   "OUTPUT_WIDTH", 0, vop_data->max_input.width);
	win->output_height_prop = drm_property_create_range(vop->drm_dev, DRM_MODE_PROP_IMMUTABLE,
							    "OUTPUT_HEIGHT", 0, vop_data->max_input.height);

	win->scale_prop = drm_property_create_range(vop->drm_dev, DRM_MODE_PROP_IMMUTABLE,
						    "SCALE_RATE", 8, 8);
	/*
	 * Support 24 bit(RGB888) or 16 bit(rgb565) color key.
	 * Bit 31 is used as a flag to disable (0) or enable
	 * color keying (1).
	 */
	if (VOP_WIN_SUPPORT(vop, win, color_key))
		win->color_key_prop = drm_property_create_range(vop->drm_dev, 0,
								"colorkey", 0, 0x80ffffff);
	if (!win->input_width_prop || !win->input_height_prop ||
	    !win->scale_prop) {
		DRM_ERROR("failed to create property\n");
		return -ENOMEM;
	}

	drm_object_attach_property(&win->base.base, win->input_width_prop, 0);
	drm_object_attach_property(&win->base.base, win->input_height_prop, 0);
	drm_object_attach_property(&win->base.base, win->output_width_prop, 0);
	drm_object_attach_property(&win->base.base, win->output_height_prop, 0);
	drm_object_attach_property(&win->base.base, win->scale_prop, 0);
	if (VOP_WIN_SUPPORT(vop, win, color_key))
		drm_object_attach_property(&win->base.base, win->color_key_prop, 0);

	return 0;
}

static int vop_of_init_display_lut(struct vop *vop)
{
	struct device_node *node = vop->dev->of_node;
	struct device_node *dsp_lut;
	u32 lut_len = vop->lut_len;
	struct property *prop;
	int length, i, j;
	int ret;

	if (!vop->lut)
		return -ENOMEM;

	dsp_lut = of_parse_phandle(node, "dsp-lut", 0);
	if (!dsp_lut)
		return -ENXIO;

	prop = of_find_property(dsp_lut, "gamma-lut", &length);
	if (!prop) {
		dev_err(vop->dev, "failed to find gamma_lut\n");
		return -ENXIO;
	}

	length >>= 2;

	if (length != lut_len) {
		u32 r, g, b;
		u32 *lut = kmalloc_array(length, sizeof(*lut), GFP_KERNEL);

		if (!lut)
			return -ENOMEM;
		ret = of_property_read_u32_array(dsp_lut, "gamma-lut", lut,
						 length);
		if (ret) {
			dev_err(vop->dev, "load gamma-lut failed\n");
			kfree(lut);
			return -EINVAL;
		}

		for (i = 0; i < lut_len; i++) {
			j = i * length / lut_len;
			r = lut[j] / length / length * lut_len / length;
			g = lut[j] / length % length * lut_len / length;
			b = lut[j] % length * lut_len / length;

			vop->lut[i] = r * lut_len * lut_len + g * lut_len + b;
		}

		kfree(lut);
	} else {
		of_property_read_u32_array(dsp_lut, "gamma-lut",
					   vop->lut, vop->lut_len);
	}
	vop->lut_active = true;

	return 0;
}

static int vop_crtc_create_feature_property(struct vop *vop, struct drm_crtc *crtc)
{
	const struct vop_data *vop_data = vop->data;

	struct drm_property *prop;
	u64 feature = 0;

	static const struct drm_prop_enum_list props[] = {
		{ ROCKCHIP_DRM_CRTC_FEATURE_ALPHA_SCALE, "ALPHA_SCALE" },
		{ ROCKCHIP_DRM_CRTC_FEATURE_HDR10, "HDR10" },
		{ ROCKCHIP_DRM_CRTC_FEATURE_DOVI, "DOVI" },
	};

	if (vop_data->feature & VOP_FEATURE_ALPHA_SCALE)
		feature |= BIT(ROCKCHIP_DRM_CRTC_FEATURE_ALPHA_SCALE);
	if (vop_data->feature & VOP_FEATURE_HDR10)
		feature |= BIT(ROCKCHIP_DRM_CRTC_FEATURE_HDR10);
	if (vop_data->feature & VOP_FEATURE_DOVI)
		feature |= BIT(ROCKCHIP_DRM_CRTC_FEATURE_DOVI);

	prop = drm_property_create_bitmask(vop->drm_dev,
					   DRM_MODE_PROP_IMMUTABLE, "FEATURE",
					   props, ARRAY_SIZE(props),
					   0xffffffff);
	if (!prop) {
		DRM_DEV_ERROR(vop->dev, "create FEATURE prop for vop%d failed\n", vop->id);
		return -ENOMEM;
	}

	vop->feature_prop = prop;
	drm_object_attach_property(&crtc->base, vop->feature_prop, feature);

	return 0;
}

static int vop_create_crtc(struct vop *vop)
{
	struct device *dev = vop->dev;
	struct drm_device *drm_dev = vop->drm_dev;
	struct rockchip_drm_private *private = drm_dev->dev_private;
	struct drm_plane *primary = NULL, *cursor = NULL, *plane, *tmp;
	struct drm_crtc *crtc = &vop->rockchip_crtc.crtc;
	struct device_node *port;
	int ret = 0;
	int i;

	/*
	 * Create drm_plane for primary and cursor planes first, since we need
	 * to pass them to drm_crtc_init_with_planes, which sets the
	 * "possible_crtcs" to the newly initialized crtc.
	 */
	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *win = &vop->win[i];

		if (win->type != DRM_PLANE_TYPE_PRIMARY &&
		    win->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		ret = vop_plane_init(vop, win, 0);
		if (ret) {
			DRM_DEV_ERROR(vop->dev, "failed to init plane\n");
			goto err_cleanup_planes;
		}

		plane = &win->base;
		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = plane;
		else if (plane->type == DRM_PLANE_TYPE_CURSOR)
			cursor = plane;
	}

	ret = drm_crtc_init_with_planes(drm_dev, crtc, primary, cursor,
					&vop_crtc_funcs, NULL);
	if (ret)
		goto err_cleanup_planes;

	drm_crtc_helper_add(crtc, &vop_crtc_helper_funcs);

	/*
	 * Create drm_planes for overlay windows with possible_crtcs restricted
	 * to the newly created crtc.
	 */
	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *win = &vop->win[i];
		unsigned long possible_crtcs = drm_crtc_mask(crtc);

		if (win->type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		ret = vop_plane_init(vop, win, possible_crtcs);
		if (ret) {
			DRM_DEV_ERROR(vop->dev, "failed to init overlay\n");
			goto err_cleanup_crtc;
		}
		vop_plane_add_properties(vop, &win->base, win);
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(vop->dev, "no port node found in %pOF\n",
			      dev->of_node);
		ret = -ENOENT;
		goto err_cleanup_crtc;
	}

	drm_flip_work_init(&vop->fb_unref_work, "fb_unref",
			   vop_fb_unref_worker);

	init_completion(&vop->dsp_hold_completion);
	init_completion(&vop->line_flag_completion);
	crtc->port = port;
	rockchip_register_crtc_funcs(crtc, &private_crtc_funcs);

	drm_object_attach_property(&crtc->base, private->soc_id_prop, vop->soc_id);
	drm_object_attach_property(&crtc->base, private->port_id_prop, vop->id);
	drm_object_attach_property(&crtc->base, private->aclk_prop, 0);
	drm_object_attach_property(&crtc->base, private->bg_prop, 0);
	drm_object_attach_property(&crtc->base, private->line_flag_prop, 0);

#define VOP_ATTACH_MODE_CONFIG_PROP(prop, v) \
	drm_object_attach_property(&crtc->base, drm_dev->mode_config.prop, v)

	VOP_ATTACH_MODE_CONFIG_PROP(tv_left_margin_property, 100);
	VOP_ATTACH_MODE_CONFIG_PROP(tv_right_margin_property, 100);
	VOP_ATTACH_MODE_CONFIG_PROP(tv_top_margin_property, 100);
	VOP_ATTACH_MODE_CONFIG_PROP(tv_bottom_margin_property, 100);
#undef VOP_ATTACH_MODE_CONFIG_PROP
	vop_crtc_create_feature_property(vop, crtc);

	if (vop->lut_regs) {
		u16 *r_base, *g_base, *b_base;
		u32 lut_len = vop->lut_len;

		vop->lut = devm_kmalloc_array(dev, lut_len, sizeof(*vop->lut),
					      GFP_KERNEL);
		if (!vop->lut) {
			ret = -ENOMEM;
			goto err_unregister_crtc_funcs;
		}

		if (vop_of_init_display_lut(vop)) {
			for (i = 0; i < lut_len; i++) {
				u32 r = i * lut_len * lut_len;
				u32 g = i * lut_len;
				u32 b = i;

				vop->lut[i] = r | g | b;
			}
		}

		drm_mode_crtc_set_gamma_size(crtc, lut_len);
		drm_crtc_enable_color_mgmt(crtc, 0, false, lut_len);
		r_base = crtc->gamma_store;
		g_base = r_base + crtc->gamma_size;
		b_base = g_base + crtc->gamma_size;

		for (i = 0; i < lut_len; i++) {
			rockchip_vop_crtc_fb_gamma_get(crtc, &r_base[i],
						       &g_base[i], &b_base[i],
						       i);
		}
	}
	return 0;

err_unregister_crtc_funcs:
	rockchip_unregister_crtc_funcs(crtc);
err_cleanup_crtc:
	drm_crtc_cleanup(crtc);
err_cleanup_planes:
	list_for_each_entry_safe(plane, tmp, &drm_dev->mode_config.plane_list,
				 head)
		drm_plane_cleanup(plane);
	return ret;
}

static void vop_destroy_crtc(struct vop *vop)
{
	struct drm_crtc *crtc = &vop->rockchip_crtc.crtc;
	struct drm_device *drm_dev = vop->drm_dev;
	struct drm_plane *plane, *tmp;

	of_node_put(crtc->port);

	/*
	 * We need to cleanup the planes now.  Why?
	 *
	 * The planes are "&vop->win[i].base".  That means the memory is
	 * all part of the big "struct vop" chunk of memory.  That memory
	 * was devm allocated and associated with this component.  We need to
	 * free it ourselves before vop_unbind() finishes.
	 */
	list_for_each_entry_safe(plane, tmp, &drm_dev->mode_config.plane_list,
				 head)
		vop_plane_destroy(plane);

	/*
	 * Destroy CRTC after vop_plane_destroy() since vop_disable_plane()
	 * references the CRTC.
	 */
	drm_crtc_cleanup(crtc);
	drm_flip_work_cleanup(&vop->fb_unref_work);
}

/*
 * Win_id is the order in vop_win_data array.
 * This is related to the actual hardware plane.
 * But in the Linux platform, such as video hardware and camera preview,
 * it can only be played on the nv12 plane.
 * So set the order of zpos to PRIMARY < OVERLAY (if have) < CURSOR (if have).
 */
static int vop_plane_get_zpos(enum drm_plane_type type, unsigned int size)
{
	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
		return 0;
	case DRM_PLANE_TYPE_OVERLAY:
		return 1;
	case DRM_PLANE_TYPE_CURSOR:
		return size - 1;
	}
	return 0;
}

/*
 * Initialize the vop->win array elements.
 */
static int vop_win_init(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	unsigned int i, j;
	unsigned int num_wins = 0;
	char name[DRM_PROP_NAME_LEN];
	uint8_t plane_id = 0;
	struct drm_prop_enum_list *plane_name_list;
	static const struct drm_prop_enum_list props[] = {
		{ ROCKCHIP_DRM_PLANE_FEATURE_SCALE, "scale" },
		{ ROCKCHIP_DRM_PLANE_FEATURE_ALPHA, "alpha" },
		{ ROCKCHIP_DRM_PLANE_FEATURE_HDR2SDR, "hdr2sdr" },
		{ ROCKCHIP_DRM_PLANE_FEATURE_SDR2HDR, "sdr2hdr" },
		{ ROCKCHIP_DRM_PLANE_FEATURE_AFBDC, "afbdc" },
	};

	for (i = 0; i < vop_data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[num_wins];
		const struct vop_win_data *win_data = &vop_data->win[i];

		if (!win_data->phy)
			continue;

		vop_win->phy = win_data->phy;
		vop_win->csc = win_data->csc;
		vop_win->offset = win_data->base;
		vop_win->type = win_data->type;
		vop_win->data_formats = win_data->phy->data_formats;
		vop_win->nformats = win_data->phy->nformats;
		vop_win->format_modifiers = win_data->format_modifiers;
		vop_win->feature = win_data->feature;
		vop_win->vop = vop;
		vop_win->win_id = i;
		vop_win->area_id = 0;
		vop_win->plane_id = plane_id++;
		snprintf(name, sizeof(name), "VOP%d-win%d-%d", vop->id, vop_win->win_id, vop_win->area_id);
		vop_win->name = devm_kstrdup(vop->dev, name, GFP_KERNEL);
		vop_win->zpos = vop_plane_get_zpos(win_data->type,
						   vop_data->win_size);

		num_wins++;

		if (!vop->support_multi_area)
			continue;

		for (j = 0; j < win_data->area_size; j++) {
			struct vop_win *vop_area = &vop->win[num_wins];
			const struct vop_win_phy *area = win_data->area[j];

			vop_area->parent = vop_win;
			vop_area->offset = vop_win->offset;
			vop_area->phy = area;
			vop_area->type = DRM_PLANE_TYPE_OVERLAY;
			vop_area->data_formats = vop_win->data_formats;
			vop_area->nformats = vop_win->nformats;
			vop_area->format_modifiers = win_data->format_modifiers;
			vop_area->vop = vop;
			vop_area->win_id = i;
			vop_area->area_id = j + 1;
			vop_area->plane_id = plane_id++;
			snprintf(name, sizeof(name), "VOP%d-win%d-%d", vop->id, vop_area->win_id, vop_area->area_id);
			vop_area->name = devm_kstrdup(vop->dev, name, GFP_KERNEL);
			num_wins++;
		}
	}

	vop->num_wins = num_wins;

	vop->plane_feature_prop = drm_property_create_bitmask(vop->drm_dev,
				DRM_MODE_PROP_IMMUTABLE, "FEATURE",
				props, ARRAY_SIZE(props),
				BIT(ROCKCHIP_DRM_PLANE_FEATURE_SCALE) |
				BIT(ROCKCHIP_DRM_PLANE_FEATURE_ALPHA) |
				BIT(ROCKCHIP_DRM_PLANE_FEATURE_HDR2SDR) |
				BIT(ROCKCHIP_DRM_PLANE_FEATURE_SDR2HDR) |
				BIT(ROCKCHIP_DRM_PLANE_FEATURE_AFBDC));
	if (!vop->plane_feature_prop) {
		DRM_ERROR("failed to create feature property\n");
		return -EINVAL;
	}

	plane_name_list = devm_kzalloc(vop->dev,
				       vop->num_wins * sizeof(*plane_name_list),
				       GFP_KERNEL);
	if (!plane_name_list) {
		DRM_DEV_ERROR(vop->dev, "failed to alloc memory for plane_name_list\n");
		return -ENOMEM;
	}

	for (i = 0; i < vop->num_wins; i++) {
		struct vop_win *vop_win = &vop->win[i];

		plane_name_list[i].type = vop_win->plane_id;
		plane_name_list[i].name = vop_win->name;
	}

	vop->plane_name_list = plane_name_list;

	return 0;
}

static int vop_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct vop_data *vop_data;
	struct drm_device *drm_dev = data;
	struct vop *vop;
	struct resource *res;
	struct device *virt_dev = NULL;
	size_t alloc_size;
	int ret, irq, i;
	int num_wins = 0;
	bool dual_channel_swap = false;
	struct device_node *mcu = NULL;

	vop_data = of_device_get_match_data(dev);
	if (!vop_data)
		return -ENODEV;

	for (i = 0; i < vop_data->win_size; i++) {
		const struct vop_win_data *win_data = &vop_data->win[i];

		num_wins += win_data->area_size + 1;
	}

	/* Allocate vop struct and its vop_win array */
	alloc_size = sizeof(*vop) + sizeof(*vop->win) * num_wins;
	vop = devm_kzalloc(dev, alloc_size, GFP_KERNEL);
	if (!vop)
		return -ENOMEM;

	vop->dev = dev;
	vop->data = vop_data;
	vop->drm_dev = drm_dev;
	vop->num_wins = num_wins;
	vop->version = vop_data->version;
	vop->soc_id = vop_data->soc_id;
	vop->id = vop_data->vop_id;
	dev_set_drvdata(dev, vop);
	vop->support_multi_area = of_property_read_bool(dev->of_node, "support-multi-area");

	ret = vop_win_init(vop);
	if (ret)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "regs");
	if (!res) {
		dev_warn(vop->dev, "failed to get vop register byname\n");
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	}
	vop->res = res;
	vop->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(vop->regs))
		return PTR_ERR(vop->regs);
	vop->len = resource_size(res);

	vop->regsbak = devm_kzalloc(dev, vop->len, GFP_KERNEL);
	if (!vop->regsbak)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gamma_lut");
	if (res) {
		vop->lut_res = res;
		vop->lut_len = resource_size(res) / sizeof(*vop->lut);
		if (vop->lut_len != 256 && vop->lut_len != 1024) {
			dev_err(vop->dev, "unsupported lut sizes %d\n",
				vop->lut_len);
			return -EINVAL;
		}
		vop->lut_regs = devm_ioremap_resource(dev, res);
		if (IS_ERR(vop->lut_regs))
			return PTR_ERR(vop->lut_regs);
	}

	vop->grf = syscon_regmap_lookup_by_phandle_optional(dev->of_node, "rockchip,grf");
	vop->vo0_grf = syscon_regmap_lookup_by_phandle_optional(dev->of_node, "rockchip,vo0-grf");

	vop->hclk = devm_clk_get(vop->dev, "hclk_vop");
	if (IS_ERR(vop->hclk)) {
		dev_err(vop->dev, "failed to get hclk source\n");
		return PTR_ERR(vop->hclk);
	}
	vop->aclk = devm_clk_get(vop->dev, "aclk_vop");
	if (IS_ERR(vop->aclk)) {
		dev_err(vop->dev, "failed to get aclk source\n");
		return PTR_ERR(vop->aclk);
	}
	vop->dclk = devm_clk_get(vop->dev, "dclk_vop");
	if (IS_ERR(vop->dclk)) {
		dev_err(vop->dev, "failed to get dclk source\n");
		return PTR_ERR(vop->dclk);
	}
	vop->dclk_source = devm_clk_get(vop->dev, "dclk_source");
	if (PTR_ERR(vop->dclk_source) == -ENOENT) {
		vop->dclk_source = NULL;
	} else if (PTR_ERR(vop->dclk_source) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (IS_ERR(vop->dclk_source)) {
		dev_err(vop->dev, "failed to get dclk source parent\n");
		return PTR_ERR(vop->dclk_source);
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		DRM_DEV_ERROR(dev, "cannot find irq for vop\n");
		return irq;
	}
	vop->irq = (unsigned int)irq;

	spin_lock_init(&vop->reg_lock);
	spin_lock_init(&vop->irq_lock);
	mutex_init(&vop->vop_lock);

	ret = vop_create_crtc(vop);
	if (ret)
		return ret;

	vop_wb_connector_init(vop);
	pm_runtime_enable(&pdev->dev);

	if (of_count_phandle_with_args(dev->of_node, "power-domains", "#power-domain-cells") > 1) {
		virt_dev = dev_pm_domain_attach_by_name(dev, "pd0");
		if (!IS_ERR(virt_dev))
			vop->genpd_dev0 = virt_dev;
		virt_dev = dev_pm_domain_attach_by_name(dev, "pd1");
		if (!IS_ERR(virt_dev))
			vop->genpd_dev1 = virt_dev;
	}

	mcu = of_get_child_by_name(dev->of_node, "mcu-timing");
	if (!mcu) {
		dev_dbg(dev, "no mcu-timing node found in %s\n",
			dev->of_node->full_name);
	} else {
		u32 val;

		if (!of_property_read_u32(mcu, "mcu-pix-total", &val))
			vop->mcu_timing.mcu_pix_total = val;
		if (!of_property_read_u32(mcu, "mcu-cs-pst", &val))
			vop->mcu_timing.mcu_cs_pst = val;
		if (!of_property_read_u32(mcu, "mcu-cs-pend", &val))
			vop->mcu_timing.mcu_cs_pend = val;
		if (!of_property_read_u32(mcu, "mcu-rw-pst", &val))
			vop->mcu_timing.mcu_rw_pst = val;
		if (!of_property_read_u32(mcu, "mcu-rw-pend", &val))
			vop->mcu_timing.mcu_rw_pend = val;
		if (!of_property_read_u32(mcu, "mcu-hold-mode", &val))
			vop->mcu_timing.mcu_hold_mode = val;
	}

	mcu = of_get_child_by_name(dev->of_node, "mcu-bypass-timing");
	if (!mcu) {
		dev_dbg(dev, "no mcu-bypass-timing node found in %s\n",
			dev->of_node->full_name);
	} else {
		u32 val;

		if (!of_property_read_u32(mcu, "mcu-pix-total", &val))
			vop->mcu_bypass_timing.mcu_pix_total = val;
		if (!of_property_read_u32(mcu, "mcu-cs-pst", &val))
			vop->mcu_bypass_timing.mcu_cs_pst = val;
		if (!of_property_read_u32(mcu, "mcu-cs-pend", &val))
			vop->mcu_bypass_timing.mcu_cs_pend = val;
		if (!of_property_read_u32(mcu, "mcu-rw-pst", &val))
			vop->mcu_bypass_timing.mcu_rw_pst = val;
		if (!of_property_read_u32(mcu, "mcu-rw-pend", &val))
			vop->mcu_bypass_timing.mcu_rw_pend = val;
		if (!of_property_read_u32(mcu, "mcu-hold-mode", &val))
			vop->mcu_bypass_timing.mcu_hold_mode = val;
	}

	dual_channel_swap = of_property_read_bool(dev->of_node,
						  "rockchip,dual-channel-swap");
	vop->dual_channel_swap = dual_channel_swap;

	rockchip_drm_dma_init_device(drm_dev, dev);

	ret = devm_request_irq(dev, vop->irq, vop_isr,
			       IRQF_SHARED, dev_name(dev), vop);
	if (ret)
		goto err_pm_detach;

	return 0;

err_pm_detach:
	if (vop->genpd_dev1)
		dev_pm_domain_detach(vop->genpd_dev1, true);
	if (vop->genpd_dev0)
		dev_pm_domain_detach(vop->genpd_dev0, true);
	pm_runtime_disable(dev);
	vop_destroy_crtc(vop);
	return ret;
}

static void vop_unbind(struct device *dev, struct device *master, void *data)
{
	struct vop *vop = dev_get_drvdata(dev);

	if (vop->genpd_dev1)
		dev_pm_domain_detach(vop->genpd_dev1, true);

	if (vop->genpd_dev0)
		dev_pm_domain_detach(vop->genpd_dev0, true);

	pm_runtime_disable(dev);
	vop_destroy_crtc(vop);

	vop_wb_connector_destory(vop);
}

const struct component_ops vop_component_ops = {
	.bind = vop_bind,
	.unbind = vop_unbind,
};
EXPORT_SYMBOL_GPL(vop_component_ops);
