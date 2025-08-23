/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * based on exynos_drm_drv.h
 */

#ifndef _ROCKCHIP_DRM_DRV_H
#define _ROCKCHIP_DRM_DRV_H

#include <drm/drm_atomic_helper.h>
#include <drm/display/drm_dsc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_edid.h>
#include <drm/drm_gem.h>
#include <drm/drm_panel.h>
#include <drm/rockchip_drm.h>

#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/debugfs.h>

#include <soc/rockchip/rockchip_dmc.h>

#include "rockchip_drm_debugfs.h"

#define ROCKCHIP_MAX_FB_BUFFER	3
#define ROCKCHIP_MAX_CONNECTOR	2
#define ROCKCHIP_MAX_CRTC	4
#define ROCKCHIP_MAX_LAYER	16
#define ROCKCHIP_MAX_DOVI_CORE	3

struct drm_device;
struct drm_connector;
struct iommu_domain;

#define VOP_COLOR_KEY_NONE	(0 << 31)
#define VOP_COLOR_KEY_MASK	(1 << 31)

#define VOP_OUTPUT_IF_RGB	BIT(0)
#define VOP_OUTPUT_IF_BT1120	BIT(1)
#define VOP_OUTPUT_IF_BT656	BIT(2)
#define VOP_OUTPUT_IF_LVDS0	BIT(3)
#define VOP_OUTPUT_IF_LVDS1	BIT(4)
#define VOP_OUTPUT_IF_MIPI0	BIT(5)
#define VOP_OUTPUT_IF_MIPI1	BIT(6)
#define VOP_OUTPUT_IF_eDP0	BIT(7)
#define VOP_OUTPUT_IF_eDP1	BIT(8)
#define VOP_OUTPUT_IF_DP0	BIT(9)
#define VOP_OUTPUT_IF_DP1	BIT(10)
#define VOP_OUTPUT_IF_HDMI0	BIT(11)
#define VOP_OUTPUT_IF_HDMI1	BIT(12)
#define VOP_OUTPUT_IF_DP2	BIT(13)

#define RK_IF_PROP_COLOR_DEPTH		"color_depth"
#define RK_IF_PROP_COLOR_FORMAT		"color_format"
#define RK_IF_PROP_COLOR_DEPTH_CAPS	"color_depth_caps"
#define RK_IF_PROP_COLOR_FORMAT_CAPS	"color_format_caps"
#define RK_IF_PROP_ENCRYPTED		"hdcp_encrypted"

/*
 * This is extend by rockchip, the other EOTF is defined at hdmi.h
 *
 * enum hdmi_eotf {
 *	HDMI_EOTF_TRADITIONAL_GAMMA_SDR,
 *	HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
 *	HDMI_EOTF_SMPTE_ST2084,
 *	HDMI_EOTF_BT_2100_HLG,
 *};
 */
#define HDMI_EOTF_HDR10PLUS	0x10
#define HDMI_EOTF_HDRVIVID	0x11
#define HDMI_EOTF_DOVI		0x12
#define DOVI_VSDB_LEN		26

enum rockchip_drm_debug_category {
	VOP_DEBUG_PLANE		= BIT(0),
	VOP_DEBUG_OVERLAY	= BIT(1),
	VOP_DEBUG_WB		= BIT(2),
	VOP_DEBUG_CFG_DONE	= BIT(3),
	VOP_DEBUG_CLK		= BIT(4),
	VOP_DEBUG_IOMMU_MAP     = BIT(5),
	VOP_DEBUG_VSYNC		= BIT(7),
	VOP_DEBUG_PIXEL_SHIFT	= BIT(8),
};

enum rk_if_color_depth {
	RK_IF_DEPTH_8,
	RK_IF_DEPTH_10,
	RK_IF_DEPTH_12,
	RK_IF_DEPTH_16,
	RK_IF_DEPTH_420_10,
	RK_IF_DEPTH_420_12,
	RK_IF_DEPTH_420_16,
	RK_IF_DEPTH_6,
	RK_IF_DEPTH_MAX,
};

enum rk_if_color_format {
	RK_IF_FORMAT_RGB, /* default RGB */
	RK_IF_FORMAT_YCBCR444, /* YCBCR 444 */
	RK_IF_FORMAT_YCBCR422, /* YCBCR 422 */
	RK_IF_FORMAT_YCBCR420, /* YCBCR 420 */
	RK_IF_FORMAT_YCBCR_HQ, /* Highest subsampled YUV */
	RK_IF_FORMAT_YCBCR_LQ, /* Lowest subsampled YUV */
	RK_IF_FORMAT_MAX,
};

enum rockchip_hdcp_encrypted {
	RK_IF_HDCP_ENCRYPTED_NONE = 0,
	RK_IF_HDCP_ENCRYPTED_LEVEL1,
	RK_IF_HDCP_ENCRYPTED_LEVEL2,
};

enum rockchip_color_bar_mode {
	ROCKCHIP_COLOR_BAR_OFF = 0,
	ROCKCHIP_COLOR_BAR_HORIZONTAL = 1,
	ROCKCHIP_COLOR_BAR_VERTICAL = 2,
	ROCKCHIP_COLOR_BAR_HORIZONTAL_COLOR_GRADIENT = 3,
	ROCKCHIP_COLOR_BAR_VERTICAL_COLOR_GRADIENT = 4,
	ROCKCHIP_COLOR_BAR_MUTANT = 5,
	ROCKCHIP_COLOR_BAR_FIX0 = 6,
	ROCKCHIP_COLOR_BAR_FIX1 = 7,
};

enum rockchip_drm_split_area {
	ROCKCHIP_DRM_SPLIT_UNSET = 0,
	ROCKCHIP_DRM_SPLIT_LEFT_SIDE = 1,
	ROCKCHIP_DRM_SPLIT_RIGHT_SIDE = 2,
};

enum rockchip_drm_vop_aclk_mode {
	ROCKCHIP_VOP_ACLK_NORMAL_MODE = 0,
	ROCKCHIP_VOP_ACLK_RESET_MODE,
	ROCKCHIP_VOP_ACLK_ADVANCED_MODE,
	ROCKCHIP_VOP_ACLK_MAX_MODE,
};

enum rockchip_drm_vrr_type {
	ROCKCHIP_VRR_VFP_MODE = 0,
	ROCKCHIP_VRR_HFP_MODE = 1,
	ROCKCHIP_VRR_DCLK_MODE = 2,
};

struct rockchip_drm_sub_dev {
	struct list_head list;
	struct drm_connector *connector;
	struct device_node *of_node;
	int (*loader_protect)(struct rockchip_drm_sub_dev *sub_dev, bool on);
	void (*update_vfp_for_vrr)(struct drm_connector *connector, struct drm_display_mode *mode,
				   int vfp);
};

struct rockchip_sdr2hdr_state {
	int sdr2hdr_func;

	bool bt1886eotf_pre_conv_en;
	bool rgb2rgb_pre_conv_en;
	bool rgb2rgb_pre_conv_mode;
	bool st2084oetf_pre_conv_en;

	bool bt1886eotf_post_conv_en;
	bool rgb2rgb_post_conv_en;
	bool rgb2rgb_post_conv_mode;
	bool st2084oetf_post_conv_en;
};

struct rockchip_hdr_state {
	bool pre_overlay;
	bool hdr2sdr_en;
	struct rockchip_sdr2hdr_state sdr2hdr_state;
};

struct rockchip_bcsh_state {
	int brightness;
	int contrast;
	int saturation;
	int sin_hue;
	int cos_hue;
};

struct rockchip_crtc {
	struct drm_crtc crtc;
	/* @frme_count: the frame num of commit buf */
	u32 frame_count;
#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
	/**
	 * @vop_dump_status the status of vop dump control
	 * @vop_dump_times control the dump times
	 * @vop_dump_frame_count the frame of dump buf
	 */
	enum vop_dump_status vop_dump_status;
	int vop_dump_times;
	int vop_dump_frame_count;
#endif
};

struct rockchip_dsc_sink_cap {
	/**
	 * @slice_width: the number of pixel columns that comprise the slice width
	 * @slice_height: the number of pixel rows that comprise the slice height
	 * @block_pred: Does block prediction
	 * @native_420: Does sink support DSC with 4:2:0 compression
	 * @bpc_supported: compressed bpc supported by sink : 10, 12 or 16 bpc
	 * @version_major: DSC major version
	 * @version_minor: DSC minor version
	 * @target_bits_per_pixel_x16: bits num after compress and multiply 16
	 */
	u16 slice_width;
	u16 slice_height;
	bool block_pred;
	bool native_420;
	u8 bpc_supported;
	u8 version_major;
	u8 version_minor;
	u16 target_bits_per_pixel_x16;
};

#define ACM_GAIN_LUT_HY_LENGTH		(9*17)
#define ACM_GAIN_LUT_HY_TOTAL_LENGTH	(ACM_GAIN_LUT_HY_LENGTH * 3)
#define ACM_GAIN_LUT_HS_LENGTH		(13*17)
#define ACM_GAIN_LUT_HS_TOTAL_LENGTH	(ACM_GAIN_LUT_HS_LENGTH * 3)
#define ACM_DELTA_LUT_H_LENGTH		65
#define ACM_DELTA_LUT_H_TOTAL_LENGTH	(ACM_DELTA_LUT_H_LENGTH * 3)

struct post_acm {
	s16 delta_lut_h[ACM_DELTA_LUT_H_TOTAL_LENGTH];
	s16 gain_lut_hy[ACM_GAIN_LUT_HY_TOTAL_LENGTH];
	s16 gain_lut_hs[ACM_GAIN_LUT_HS_TOTAL_LENGTH];
	u16 y_gain;
	u16 h_gain;
	u16 s_gain;
	u16 acm_enable;
};

struct post_csc {
	u16 hue;
	u16 saturation;
	u16 contrast;
	u16 brightness;
	u16 r_gain;
	u16 g_gain;
	u16 b_gain;
	u16 r_offset;
	u16 g_offset;
	u16 b_offset;
	u16 csc_enable;
};

#define ROCKCHIP_VOP_DCI_LUT_LENGTH 5632

struct dci_data {
	u32 plat; /* Reserved to distinguish later platform */
	u8 dci_lut_data[ROCKCHIP_VOP_DCI_LUT_LENGTH];
	u32 blk_size_h_ratio;
	u32 blk_size_v_ratio;
	u32 dci_act_w;
	u32 dci_act_h;
	u32 adj0;
	u32 adj1;
	u32 uv_adj;
	u32 dci_en;
};

#define SHARP_REG_LENGTH 692

struct post_sharp {
	u32 plat; /* Reserved to distinguish later platform */
	u32 regs[SHARP_REG_LENGTH / 4];
};

struct rockchip_hdmi_vrr_state {
	bool refresh_rate_ready_to_change;
	bool m_const;
	u8 next_tfr_val;
	u8 fva_factor_m1_val;
	unsigned int vrr_frame_cnt;
	const struct mvrr_const_val *mconst_val;
};

struct rockchip_crtc_state {
	struct drm_crtc_state base;
	int vp_id;
	int output_type;
	int output_mode;
	int output_bpc;
	int output_flags;
	int data_map_mode;
	bool enable_afbc;
	bool yuv_overlay;
	/**
	 * @splice_mode: enabled when display a hdisplay > 4096 on rk3588
	 */
	bool splice_mode;

	/**
	 * @hold_mode: enabled when it's:
	 * (1) mcu hold mode
	 * (2) mipi dsi cmd mode
	 * (3) edp psr mode
	 */
	bool hold_mode;
	/**
	 * when enable soft_te, use gpio irq to triggle new fs,
	 * otherwise use hardware te
	 */
	bool soft_te;
	/**
	 * sharp and post scaler share line buffer,
	 * So they are mutually exclusive.
	 */
	bool sharp_en;

	bool dimming_changed;

	struct drm_tv_connector_state *tv_state;
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;
	int vdisplay;
	int afbdc_win_format;
	int afbdc_win_width;
	int afbdc_win_height;
	int afbdc_win_ptr;
	int afbdc_win_id;
	int afbdc_en;
	int afbdc_win_vir_width;
	int afbdc_win_xoffset;
	int afbdc_win_yoffset;
	int dsp_layer_sel;
	u32 output_if;
	u32 output_if_left_panel;
	u32 bus_format;
	u32 bus_flags;
	int post_r2y_en;
	int post_y2r_en;
	int post_csc_mode;
	int bcsh_en;
	enum drm_color_encoding color_encoding;
	enum drm_color_range color_range;
	int eotf;
	u32 background;
	u32 line_flag;
	u8 mode_update;
	u8 dsc_id;
	u8 dsc_enable;

	u8 dsc_slice_num;
	u8 dsc_pixel_num;

	u64 dsc_txp_clk_rate;
	u64 dsc_pxl_clk_rate;
	u64 dsc_cds_clk_rate;

	struct drm_dsc_picture_parameter_set pps;
	struct rockchip_dsc_sink_cap dsc_sink_cap;
	struct rockchip_hdr_state hdr;
	struct rockchip_hdmi_vrr_state hdmi_vrr;
	struct drm_property_blob *hdr_ext_data;
	struct drm_property_blob *acm_lut_data;
	struct drm_property_blob *post_csc_data;
	struct drm_property_blob *post_sharp_data;
	struct drm_property_blob *cubic_lut_data;
	struct drm_property_blob *dimming_data;

	int request_refresh_rate;
	int max_refresh_rate;
	int min_refresh_rate;
	int shift_x;
	int shift_y;
	int vrr_type;
};

#define to_rockchip_crtc_state(s) \
		container_of(s, struct rockchip_crtc_state, base)

struct rockchip_drm_vcnt {
	struct drm_pending_vblank_event *event;
	__u32 sequence;
	int pipe;
};

struct rockchip_drm_error_event {
	wait_queue_head_t wait;
	struct task_struct *thread;
	struct list_head event_list;
	struct drm_event_vblank event;
	bool error_state;
	spinlock_t lock;
};

struct rockchip_logo {
	dma_addr_t dma_addr;
	struct drm_mm_node logo_reserved_node;
	void *kvaddr;
	phys_addr_t start;
	phys_addr_t size;
	int count;
};

struct rockchip_mcu_timing {
	int mcu_pix_total;
	int mcu_cs_pst;
	int mcu_cs_pend;
	int mcu_rw_pst;
	int mcu_rw_pend;
	int mcu_hold_mode;
};

struct vop_mcu_bypass_cfg {
	const struct rockchip_mcu_timing timing;
	unsigned long dclk_rate;
};

struct loader_cubic_lut {
	bool enable;
	u32 offset;
};

struct rockchip_drm_dsc_cap {
	bool v_1p2;
	bool native_420;
	bool all_bpp;
	u8 bpc_supported;
	u8 max_slices;
	u8 max_lanes;
	u8 max_frl_rate_per_lane;
	u8 total_chunk_kbytes;
	int clk_per_slice;
};

struct ver_26_v0 {
	u8 yuv422_12bit;
	u8 support_2160p_60;
	u8 global_dimming;
	u8 dm_major_ver;
	u8 dm_minor_ver;
	u16 t_min_pq;
	u16 t_max_pq;
	u16 rx;
	u16 ry;
	u16 gx;
	u16 gy;
	u16 bx;
	u16 by;
	u16 wx;
	u16 wy;
} __packed;

struct ver_15_v1 {
	u8 yuv422_12bit;
	u8 support_2160p_60;
	u8 global_dimming;
	u8 dm_version;
	u8 colorimetry;
	u8 t_max_lum;
	u8 t_min_lum;
	u8 rx;
	u8 ry;
	u8 gx;
	u8 gy;
	u8 bx;
	u8 by;
} __packed;

struct ver_12_v1 {
	u8 yuv422_12bit;
	u8 support_2160p_60;
	u8 global_dimming;
	u8 dm_version;
	u8 colorimetry;
	u8 low_latency;
	u8 t_max_lum;
	u8 t_min_lum;
	u8 unique_rx;
	u8 unique_ry;
	u8 unique_gx;
	u8 unique_gy;
	u8 unique_bx;
	u8 unique_by;
} __packed;

struct ver_12_v2 {
	u8 yuv422_12bit;
	u8 backlt_ctrl;
	u8 global_dimming;
	u8 dm_version;
	u8 backlt_min_luma;
	u8 interface;
	u8 yuv444_10b_12b;
	u8 t_min_pq_v2;
	u8 t_max_pq_v2;
	u8 unique_rx;
	u8 unique_ry;
	u8 unique_gx;
	u8 unique_gy;
	u8 unique_bx;
	u8 unique_by;
} __packed;

struct next_hdr_sink_data {
	u8 version;
	struct ver_26_v0 ver_26_v0;
	struct ver_15_v1 ver_15_v1;
	struct ver_12_v1 ver_12_v1;
	struct ver_12_v2 ver_12_v2;
} __packed;

/* refer to HDMI2.1B P453 */
enum TARGET_FRAME_RATE {
	TFR_QMSVRR_INACTIVE = 0,
	TFR_23P97,
	TFR_24,
	TFR_25,
	TFR_29P97,
	TFR_30,
	TFR_47P95,
	TFR_48,
	TFR_50,
	TFR_59P94,
	TFR_60,
	TFR_100,
	TFR_119P88,
	TFR_120,
	TFR_MAX,
};

enum hdmi_brr_vic {
	HDMI_16_1920x1080P60_16x9 = 16,
	HDMI_63_1920x1080P120_16x9 = 63,
	HDMI_4_1280x720P60_16x9 = 4,
	HDMI_47_1280x720P120_16x9 = 47,
	HDMI_97_3840x2160P60_16x9 = 97,
	HDMI_102_4096x2160P60_256x135 = 102,
};

struct mvrr_const_val {
	/* unit: 100  6000, 5994, 5000, 3000, 2997, 2500, 2400, 2397 */
	u16 vrefresh_khz;
	u16 vtotal_fixed; /* vtotal_fixed is mutex with bit_len */
	u8 bit_len; /* current max value is 16 * 8, 128 */
	u8 frac_array[16];
};

struct mvrr_const_st {
	enum hdmi_brr_vic brr_vic; /* the vic of brr */
	const struct mvrr_const_val *val[];
};

/*
 * Rockchip drm private crtc funcs.
 * @loader_protect: protect loader logo crtc's power
 * @enable_vblank: enable crtc vblank irq.
 * @disable_vblank: disable crtc vblank irq.
 * @bandwidth: report present crtc bandwidth consume.
 * @cancel_pending_vblank: cancel pending vblank.
 * @debugfs_dump: debugfs to dump crtc and plane state.
 * @regs_dump: dump vop current register config.
 * @mode_valid: verify that the current mode is supported.
 * @crtc_close: close vop.
 * @crtc_send_mcu_cmd: send mcu panel init cmd.
 * @te_handler: soft te hand for cmd mode panel.
 * @wait_vact_end: wait the last active line.
 */
struct rockchip_crtc_funcs {
	int (*loader_protect)(struct drm_crtc *crtc, bool on, void *data);
	int (*enable_vblank)(struct drm_crtc *crtc);
	void (*disable_vblank)(struct drm_crtc *crtc);
	size_t (*bandwidth)(struct drm_crtc *crtc,
			    struct drm_crtc_state *crtc_state,
			    struct dmcfreq_vop_info *vop_bw_info);
	void (*cancel_pending_vblank)(struct drm_crtc *crtc,
				      struct drm_file *file_priv);
	int (*sysfs_init)(struct device *dev, struct drm_crtc *crtc);
	int (*sysfs_fini)(struct device *dev, struct drm_crtc *crtc);
	int (*debugfs_dump)(struct drm_crtc *crtc, struct seq_file *s);
	void (*regs_dump)(struct drm_crtc *crtc, struct seq_file *s);
	void (*active_regs_dump)(struct drm_crtc *crtc, struct seq_file *s);
	int (*regs_write)(struct drm_crtc *crtc, phys_addr_t address, u32 val);
	enum drm_mode_status (*mode_valid)(struct drm_crtc *crtc,
					   const struct drm_display_mode *mode,
					   int output_type);
	void (*crtc_close)(struct drm_crtc *crtc);
	void (*crtc_send_mcu_cmd)(struct drm_crtc *crtc, u32 type, u32 value);
	void (*te_handler)(struct drm_crtc *crtc);
	int (*wait_vact_end)(struct drm_crtc *crtc, unsigned int mstimeout);
	void (*crtc_standby)(struct drm_crtc *crtc, bool standby);
	void (*crtc_output_post_enable)(struct drm_crtc *crtc, int intf);
	void (*crtc_output_pre_disable)(struct drm_crtc *crtc, int intf);
	int (*crtc_set_color_bar)(struct drm_crtc *crtc, enum rockchip_color_bar_mode mode);
	unsigned long (*crtc_get_dclk_rate)(struct drm_crtc *crtc);
	int (*set_aclk)(struct drm_crtc *crtc, enum rockchip_drm_vop_aclk_mode aclk_mode, struct dmcfreq_vop_info *vop_bw_info);
	int (*get_crc)(struct drm_crtc *crtc);
	void (*iommu_fault_handler)(struct drm_crtc *crtc, struct iommu_domain *iommu);
};

struct rockchip_dclk_pll {
	struct clk *pll;
	unsigned int use_count;
};

/*
 * Rockchip drm private structure.
 *
 * @crtc: array of enabled CRTCs, used to map from "pipe" to drm_crtc.
 * @num_pipe: number of pipes for this device.
 * @mm_lock: protect drm_mm on multi-threads.
 */
struct rockchip_drm_private {
	struct rockchip_logo *logo;
	struct drm_fb_helper *fbdev_helper;
	struct drm_gem_object *fbdev_bo;
	struct iommu_domain *domain;
	struct device *iommu_dev;
	struct gen_pool *secure_buffer_pool;
	struct mutex mm_lock;
	struct drm_mm mm;

	/* private crtc prop */
	struct drm_property *soc_id_prop;
	struct drm_property *port_id_prop;
	struct drm_property *aclk_prop;
	struct drm_property *bg_prop;
	struct drm_property *line_flag_prop;
	struct drm_property *cubic_lut_prop;
	struct drm_property *cubic_lut_size_prop;

	/* private plane prop */
	struct drm_property *eotf_prop;
	struct drm_property *async_commit_prop;
	struct drm_property *share_id_prop;
	struct drm_property *dovi_input_type_prop;

	/* private connector prop */
	struct drm_property *connector_id_prop;
	struct drm_property *split_area_prop;

	/* private local dimming prop */
	struct drm_property *dimming_data_prop;

	const struct rockchip_crtc_funcs *crtc_funcs[ROCKCHIP_MAX_CRTC];

	uint64_t iommu_fault_count;

	struct rockchip_dclk_pll default_pll;
	struct rockchip_dclk_pll hdmi_pll;

	/*
	 * protect some shared overlay resource
	 * OVL_LAYER_SEL/OVL_PORT_SEL
	 */
	struct mutex ovl_lock;
	bool need_ovl_lock;
	/**
	 * @dovi_mode: dovi mode from userspace for dovi test.
	 */
	u8 dovi_mode;

	struct rockchip_drm_vcnt vcnt[ROCKCHIP_MAX_CRTC];
	struct rockchip_drm_error_event error_event;

	/**
	 * @loader_protect
	 * ignore restore_fbdev_mode_atomic when in logo on state
	 */
	bool loader_protect;

	dma_addr_t cubic_lut_dma_addr;
	void *cubic_lut_kvaddr;
	int aclk_adjust_frame_num;
	struct drm_mm_node *clut_reserved_node;
	struct loader_cubic_lut cubic_lut[ROCKCHIP_MAX_CRTC];

	struct device *sysfs_devs[ROCKCHIP_MAX_CRTC];
};

struct rockchip_encoder {
	int crtc_endpoint_id;
	struct drm_encoder encoder;
};

struct rockchip_drm_vrr_cap {
	bool qms;
	bool m_delta;
	bool cinema_vrr;
	bool negm_vrr;
	bool fva;
	bool qms_tfr_max;
	bool qms_tfr_min;
	u8 vrr_min;
	u16 vrr_max;
};

struct rockchip_drm_hdmi21_data {
	u8 max_frl_rate_per_lane;
	u8 max_lanes;
	bool allm_supported;
	struct rockchip_drm_dsc_cap dsc_cap;
	struct rockchip_drm_vrr_cap vrr_cap;
};

void rockchip_connector_update_vfp_for_vrr(struct drm_crtc *crtc, struct drm_display_mode *mode,
					   int vfp);
int rockchip_drm_dma_attach_device(struct drm_device *drm_dev,
				   struct device *dev);
void rockchip_drm_dma_detach_device(struct drm_device *drm_dev,
				    struct device *dev);
void rockchip_drm_dma_init_device(struct drm_device *drm_dev,
				  struct device *dev);
int rockchip_drm_wait_vact_end(struct drm_crtc *crtc, unsigned int mstimeout);
int rockchip_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct rockchip_crtc_funcs *crtc_funcs);
void rockchip_unregister_crtc_funcs(struct drm_crtc *crtc);
void rockchip_drm_crtc_standby(struct drm_crtc *crtc, bool standby);
void rockchip_drm_crtc_output_post_enable(struct drm_crtc *crtc, int intf);
void rockchip_drm_crtc_output_pre_disable(struct drm_crtc *crtc, int intf);

void rockchip_drm_register_sub_dev(struct rockchip_drm_sub_dev *sub_dev);
void rockchip_drm_unregister_sub_dev(struct rockchip_drm_sub_dev *sub_dev);
struct rockchip_drm_sub_dev *rockchip_drm_get_sub_dev(struct device_node *node);
int rockchip_drm_add_modes_noedid(struct drm_connector *connector);
void rockchip_drm_te_handle(struct drm_crtc *crtc);
struct drm_crtc *drm_atomic_get_old_crtc_for_encoder(struct drm_atomic_state *state,
						     struct drm_encoder *encoder);
struct drm_crtc *drm_atomic_get_new_crtc_for_encoder(struct drm_atomic_state *state,
						     struct drm_encoder *encoder);
void drm_mode_convert_to_split_mode(struct drm_display_mode *mode);
void drm_mode_convert_to_origin_mode(struct drm_display_mode *mode);
const char *rockchip_drm_get_color_encoding_name(enum drm_color_encoding encoding);
const char *rockchip_drm_get_color_range_name(enum drm_color_range range);
#if IS_REACHABLE(CONFIG_DRM_ROCKCHIP)
int rockchip_drm_get_sub_dev_type(void);
u32 rockchip_drm_get_scan_line_time_ns(void);
#else
static inline int rockchip_drm_get_sub_dev_type(void)
{
	return DRM_MODE_CONNECTOR_Unknown;
}

static inline u32 rockchip_drm_get_scan_line_time_ns(void)
{
	return 0;
}
#endif

int rockchip_drm_endpoint_is_subdriver(struct device_node *ep);
uint32_t rockchip_drm_of_find_possible_crtcs(struct drm_device *dev,
					     struct device_node *port);
uint32_t rockchip_drm_get_bpp(const struct drm_format_info *info);
uint32_t rockchip_drm_get_cycles_per_pixel(uint32_t bus_format);
int rockchip_drm_get_yuv422_format(struct drm_connector *connector,
				   const struct edid *edid);
int rockchip_drm_parse_cea_ext(struct rockchip_drm_hdmi21_data *hdmi21_data,
			       const struct edid *edid);
int rockchip_drm_parse_dovi(u8 *sink_data, const struct edid *edid);
int rockchip_drm_parse_colorimetry_data_block(u8 *colorimetry, const struct edid *edid);
u8 rockchip_drm_parse_hdr10_plus_vsdb(const struct edid *edid);
struct dma_buf *rockchip_drm_gem_prime_export(struct drm_gem_object *obj, int flags);
long rockchip_drm_dclk_round_rate(u32 version, struct clk *dclk, unsigned long rate);
int rockchip_drm_dclk_set_rate(u32 version, struct clk *dclk, unsigned long rate);
bool rockchip_drm_is_afbc(struct drm_plane *plane, u64 modifier);
bool rockchip_drm_is_rfbc(struct drm_plane *plane, u64 modifier);
const char *rockchip_drm_modifier_to_string(uint64_t modifier);
void rockchip_drm_reset_iommu_fault_handler_rate_limit(void);
void rockchip_drm_send_error_event(struct rockchip_drm_private *priv,
				   enum rockchip_drm_error_event_type event);
int rockchip_drm_panel_loader_protect(struct drm_panel *panel, bool on);

__printf(3, 4)
void rockchip_drm_dbg(const struct device *dev,
		      enum rockchip_drm_debug_category category,
		      const char *format, ...);
__printf(3, 4)
void rockchip_drm_dbg_thread_info(const struct device *dev,
				  enum rockchip_drm_debug_category category,
				  const char *format, ...);
u16 rockchip_hdmi_vrr_tfr_match_to_vrefresh(u8 tfr);
const struct
mvrr_const_val *rockchip_hdmi_vrr_get_vrrconf_mconst(enum hdmi_brr_vic brr_vic, u16 vrefresh_khz);
u16 rockchip_hdmi_vrr_calc_new_vtotal(const struct mvrr_const_val *mvrr, u32 frame_cnt);

extern struct platform_driver cdn_dp_driver;
extern struct platform_driver dw_hdmi_rockchip_pltfm_driver;
extern struct platform_driver dw_mipi_dsi_rockchip_driver;
extern struct platform_driver dw_mipi_dsi2_rockchip_driver;
extern struct platform_driver inno_hdmi_driver;
extern struct platform_driver rockchip_dp_driver;
extern struct platform_driver rockchip_lvds_driver;
extern struct platform_driver vop_platform_driver;
extern struct platform_driver rk3066_hdmi_driver;
extern struct platform_driver vop2_platform_driver;
extern struct platform_driver rockchip_rgb_driver;
extern struct platform_driver rockchip_tve_driver;
extern struct platform_driver dw_dp_driver;
extern struct platform_driver vconn_platform_driver;
extern struct platform_driver rockchip_vkms_platform_driver;

static inline struct rockchip_encoder *to_rockchip_encoder(struct drm_encoder *encoder)
{
	return container_of(encoder, struct rockchip_encoder, encoder);
}

#endif /* _ROCKCHIP_DRM_DRV_H_ */
