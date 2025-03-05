// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys DesignWare Cores DisplayPort Transmitter Controller
 *
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 *
 * Author: Wyon Bi <bivvy.bi@rock-chips.com>
 *	   Zhang Yubing <yubing.zhang@rock-chips.com>
 */

#include <asm/unaligned.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/display/drm_hdcp_helper.h>
#include <drm/display/drm_hdmi_helper.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_file.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/extcon-provider.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/gpio/consumer.h>
#include <linux/phy/phy.h>
#include <linux/mfd/syscon.h>
#include <linux/rockchip/rockchip_sip.h>
#include <linux/soc/rockchip/rk_vendor_storage.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>

#include <sound/hdmi-codec.h>

#include <uapi/linux/videodev2.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"
#include "rockchip_dp_mst_aux_client.h"

#define DPTX_VERSION_NUMBER			0x0000
#define DPTX_VERSION_TYPE			0x0004
#define DPTX_ID					0x0008

#define DPTX_CONFIG_REG1			0x0100
#define DPTX_CONFIG_REG2			0x0104
#define DPTX_CONFIG_REG3			0x0108

#define DPTX_CCTL				0x0200
#define DPTX_CCTL_INITIATE_MST_ACT		BIT(28)
#define ENABLE_MST_MODE				BIT(25)
#define FORCE_HPD				BIT(4)
#define DEFAULT_FAST_LINK_TRAIN_EN		BIT(2)
#define ENHANCE_FRAMING_EN			BIT(1)
#define SCRAMBLE_DIS				BIT(0)
#define DPTX_SOFT_RESET_CTRL			0x0204
#define VIDEO_RESET				BIT(5)
#define AUX_RESET				BIT(4)
#define AUDIO_SAMPLER_RESET			BIT(3)
#define HDCP_MODULE_RESET			BIT(2)
#define PHY_SOFT_RESET				BIT(1)
#define CONTROLLER_RESET			BIT(0)

#define DPTX_MST_VCP_TABLE_REG_N(n)		(0x210 + (n) * 4)

#define DPTX_VSAMPLE_CTRL_N(n)			(0x0300 + 0x10000 * (n))
#define PIXEL_MODE_SELECT			GENMASK(22, 21)
#define VIDEO_MAPPING				GENMASK(20, 16)
#define VIDEO_STREAM_ENABLE			BIT(5)
#define DPTX_VSAMPLE_STUFF_CTRL1_N(n)		(0x0304 + 0x10000 * (n))
#define DPTX_VSAMPLE_STUFF_CTRL2_N(n)		(0x0308 + 0x10000 * (n))
#define DPTX_VINPUT_POLARITY_CTRL_N(n)		(0x030c + 0x10000 * (n))
#define DE_IN_POLARITY				BIT(2)
#define HSYNC_IN_POLARITY			BIT(1)
#define VSYNC_IN_POLARITY			BIT(0)
#define DPTX_VIDEO_CONFIG1_N(n)			(0x0310 + 0x10000 * (n))
#define HACTIVE					GENMASK(31, 16)
#define HBLANK					GENMASK(15, 2)
#define I_P					BIT(1)
#define R_V_BLANK_IN_OSC			BIT(0)
#define DPTX_VIDEO_CONFIG2_N(n)			(0x0314 + 0x10000 * (n))
#define VBLANK					GENMASK(31, 16)
#define VACTIVE					GENMASK(15, 0)
#define DPTX_VIDEO_CONFIG3_N(n)			(0x0318 + 0x10000 * (n))
#define H_SYNC_WIDTH				GENMASK(31, 16)
#define H_FRONT_PORCH				GENMASK(15, 0)
#define DPTX_VIDEO_CONFIG4_N(n)			(0x031c + 0x10000 * (n))
#define V_SYNC_WIDTH				GENMASK(31, 16)
#define V_FRONT_PORCH				GENMASK(15, 0)
#define DPTX_VIDEO_CONFIG5_N(n)			(0x0320 + 0x10000 * (n))
#define INIT_THRESHOLD_HI			GENMASK(22, 21)
#define AVERAGE_BYTES_PER_TU_FRAC		GENMASK(19, 16)
#define MST_AVERAGE_BYTES_PER_TU_FRAC		GENMASK(19, 14)
#define INIT_THRESHOLD				GENMASK(13, 7)
#define AVERAGE_BYTES_PER_TU			GENMASK(6, 0)
#define DPTX_VIDEO_MSA1_N(n)			(0x0324 + 0x10000 * (n))
#define VSTART					GENMASK(31, 16)
#define HSTART					GENMASK(15, 0)
#define DPTX_VIDEO_MSA2_N(n)			(0x0328 + 0x10000 * (n))
#define MISC0					GENMASK(31, 24)
#define DPTX_VIDEO_MSA3_N(n)			(0x032c + 0x10000 * (n))
#define MISC1					GENMASK(31, 24)
#define DPTX_VIDEO_HBLANK_INTERVAL_N(n)		(0x0330 + 0x10000 * (n))
#define HBLANK_INTERVAL_EN			BIT(16)
#define HBLANK_INTERVAL				GENMASK(15, 0)

#define DPTX_AUD_CONFIG1_N(n)			(0x0400 + 0x10000 * (n))
#define AUDIO_TIMESTAMP_VERSION_NUM		GENMASK(29, 24)
#define AUDIO_PACKET_ID				GENMASK(23, 16)
#define AUDIO_MUTE				BIT(15)
#define NUM_CHANNELS				GENMASK(14, 12)
#define HBR_MODE_ENABLE				BIT(10)
#define AUDIO_DATA_WIDTH			GENMASK(9, 5)
#define AUDIO_DATA_IN_EN			GENMASK(4, 1)
#define AUDIO_INF_SELECT			BIT(0)

#define DPTX_SDP_VERTICAL_CTRL_N(n)		(0x0500 + 0x10000 * (n))
#define EN_VERTICAL_SDP				BIT(2)
#define EN_AUDIO_STREAM_SDP			BIT(1)
#define EN_AUDIO_TIMESTAMP_SDP			BIT(0)
#define DPTX_SDP_HORIZONTAL_CTRL_N(n)		(0x0504 + 0x10000 * (n))
#define EN_HORIZONTAL_SDP			BIT(2)
#define DPTX_SDP_STATUS_REGISTER_N(n)		(0x0508 + 0x10000 * (n))
#define DPTX_SDP_MANUAL_CTRL_N(n)		(0x050c + 0x10000 * (n))
#define DPTX_SDP_STATUS_EN_N(n)			(0x0510 + 0x10000 * (n))

#define DPTX_SDP_REGISTER_BANK_N(n)		(0x0600 + 0x10000 * (n))
#define SDP_REGS				GENMASK(31, 0)

#define DPTX_PHYIF_CTRL				0x0a00
#define PHY_WIDTH				BIT(25)
#define PHY_POWERDOWN				GENMASK(20, 17)
#define PHY_BUSY				GENMASK(15, 12)
#define SSC_DIS					BIT(16)
#define XMIT_ENABLE				GENMASK(11, 8)
#define PHY_LANES				GENMASK(7, 6)
#define PHY_RATE				GENMASK(5, 4)
#define TPS_SEL					GENMASK(3, 0)
#define DPTX_PHY_TX_EQ				0x0a04
#define DPTX_CUSTOMPAT0				0x0a08
#define DPTX_CUSTOMPAT1				0x0a0c
#define DPTX_CUSTOMPAT2				0x0a10
#define DPTX_HBR2_COMPLIANCE_SCRAMBLER_RESET	0x0a14
#define DPTX_PHYIF_PWRDOWN_CTRL			0x0a18

#define DPTX_AUX_CMD				0x0b00
#define AUX_CMD_TYPE				GENMASK(31, 28)
#define AUX_ADDR				GENMASK(27, 8)
#define I2C_ADDR_ONLY				BIT(4)
#define AUX_LEN_REQ				GENMASK(3, 0)
#define DPTX_AUX_STATUS				0x0b04
#define AUX_TIMEOUT				BIT(17)
#define AUX_BYTES_READ				GENMASK(23, 19)
#define AUX_STATUS				GENMASK(7, 4)
#define DPTX_AUX_DATA0				0x0b08
#define DPTX_AUX_DATA1				0x0b0c
#define DPTX_AUX_DATA2				0x0b10
#define DPTX_AUX_DATA3				0x0b14

#define DPTX_GENERAL_INTERRUPT			0x0d00
#define VIDEO_FIFO_OVERFLOW_STREAM3		BIT(26)
#define VIDEO_FIFO_OVERFLOW_STREAM2		BIT(20)
#define VIDEO_FIFO_OVERFLOW_STREAM1		BIT(14)
#define VIDEO_FIFO_OVERFLOW_STREAM0		BIT(6)
#define AUDIO_FIFO_OVERFLOW_STREAM0		BIT(5)
#define SDP_EVENT_STREAM0			BIT(4)
#define AUX_CMD_INVALID				BIT(3)
#define HDCP_EVENT				BIT(2)
#define AUX_REPLY_EVENT				BIT(1)
#define HPD_EVENT				BIT(0)
#define DPTX_GENERAL_INTERRUPT_ENABLE		0x0d04
#define HDCP_EVENT_EN				BIT(2)
#define AUX_REPLY_EVENT_EN			BIT(1)
#define HPD_EVENT_EN				BIT(0)
#define DPTX_HPD_STATUS				0x0d08
#define HPD_STATE				GENMASK(11, 9)
#define HPD_STATUS				BIT(8)
#define HPD_HOT_UNPLUG				BIT(2)
#define HPD_HOT_PLUG				BIT(1)
#define HPD_IRQ					BIT(0)
#define DPTX_HPD_INTERRUPT_ENABLE		0x0d0c
#define HPD_UNPLUG_ERR_EN			BIT(3)
#define HPD_UNPLUG_EN				BIT(2)
#define HPD_PLUG_EN				BIT(1)
#define HPD_IRQ_EN				BIT(0)

#define DPTX_HDCPCFG				0x0e00
#define DPCD12PLUS				BIT(7)
#define CP_IRQ					BIT(6)
#define BYPENCRYPTION				BIT(5)
#define HDCP_LOCK				BIT(4)
#define ENCRYPTIONDISABLE			BIT(3)
#define ENABLE_HDCP_13				BIT(2)
#define ENABLE_HDCP				BIT(1)
#define DPTX_HDCPOBS				0x0e04
#define HDCP22_RE_AUTHENTICATION_REQ		BIT(31)
#define HDCP22_AUTHENTICATION_FAILED		BIT(30)
#define HDCP22_AUTHENTICATION_SUCCESS		BIT(29)
#define HDCP22_CAPABLE_SINK			BIT(28)
#define HDCP22_SINK_CAP_CHECK_COMPLETE		BIT(27)
#define HDCP22_STATE				GENMASK(26, 24)
#define HDCP22_BOOTED				BIT(23)
#define HDCP13_BSTATUS				GENMASK(22, 19)
#define REPEATER				BIT(18)
#define HDCP_CAPABLE				BIT(17)
#define STATEE					GENMASK(16, 14)
#define STATEOEG				GENMASK(13, 11)
#define STATER					GENMASK(10, 8)
#define STATEA					GENMASK(7, 4)
#define SUBSTATEA				GENMASK(3, 1)
#define HDCPENGAGED				BIT(0)
#define DPTX_HDCPAPIINTCLR			0x0e08
#define DPTX_HDCPAPIINTSTAT			0x0e0c
#define DPTX_HDCPAPIINTMSK			0x0e10
#define HDCP22_GPIOINT				BIT(8)
#define HDCP_ENGAGED				BIT(7)
#define HDCP_FAILED				BIT(6)
#define KSVSHA1CALCDONEINT			BIT(5)
#define AUXRESPNACK7TIMES			BIT(4)
#define AUXRESPTIMEOUT				BIT(3)
#define AUXRESPDEFER7TIMES			BIT(2)
#define KSVACCESSINT				BIT(0)
#define DPTX_HDCPKSVMEMCTRL			0x0e18
#define KSVSHA1STATUS				BIT(4)
#define KSVMEMACCESS				BIT(1)
#define KSVMEMREQUEST				BIT(0)
#define DPTX_HDCPREG_BKSV0			0x3600
#define DPTX_HDCPREG_BKSV1			0x3604
#define DPTX_HDCPREG_ANCONF			0x3608
#define OANBYPASS				BIT(0)
#define DPTX_HDCPREG_AN0			0x360c
#define DPTX_HDCPREG_AN1			0x3610
#define DPTX_HDCPREG_RMLCTL			0x3614
#define ODPK_DECRYPT_ENABLE			BIT(0)
#define DPTX_HDCPREG_RMLSTS			0x3618
#define IDPK_WR_OK_STS				BIT(6)
#define	IDPK_DATA_INDEX				GENMASK(5, 0)
#define DPTX_HDCPREG_SEED			0x361c
#define DPTX_HDCPREG_DPK0			0x3620
#define DPTX_HDCPREG_DPK1			0x3624
#define DPTX_HDCP22GPIOSTS			0x3628
#define DPTX_HDCP22GPIOCHNGSTS			0x362c
#define DPTX_HDCPREG_DPK_CRC			0x3630

#define HDCP_KEY_SIZE				308
#define HDCP_KEY_SEED_SIZE			2

#define HDCP_DATA_SIZE				330
#define DP_HDCP1X_ID				6

#define HDCP_SIG_MAGIC				0x4B534541	/* "AESK" */
#define HDCP_FLG_AES				1

#define DPTX_MAX_REGISTER			DPTX_HDCPREG_DPK_CRC

#define SDP_REG_BANK_SIZE			16

#define DPTX_MAX_STREAMS			4

enum {
	RK3576_DP,
	RK3588_DP,
};

enum {
	HDCP_TX_NONE,
	HDCP_TX_1,
	HDCP_TX_2,
};

struct dw_dp_hdcp {
	struct delayed_work check_work;
	struct work_struct prop_work;
	struct mutex mutex;
	u64 value;
	unsigned long check_link_interval;
	int status;
	u8 hdcp_content_type;
	bool hdcp2_encrypted;
	bool hdcp_encrypted;
	bool is_repeater;
};

struct drm_dp_link_caps {
	bool enhanced_framing;
	bool tps3_supported;
	bool tps4_supported;
	bool fast_training;
	bool channel_coding;
	bool ssc;
};

struct drm_dp_link_train_set {
	unsigned int voltage_swing[4];
	unsigned int pre_emphasis[4];
	bool voltage_max_reached[4];
	bool pre_max_reached[4];
};

struct drm_dp_link_train {
	struct drm_dp_link_train_set request;
	struct drm_dp_link_train_set adjust;
	bool clock_recovered;
	bool channel_equalized;
};

struct dw_dp_link {
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	unsigned char revision;
	unsigned int max_rate;
	unsigned int rate;
	unsigned int lanes;
	struct drm_dp_link_caps caps;
	struct drm_dp_link_train train;
	struct drm_dp_desc desc;
	u8 sink_count;
	u8 vsc_sdp_extension_for_colorimetry_supported;
	bool sink_support_mst;
};

struct dw_dp_video {
	struct drm_display_mode mode;
	u32 bus_format;
	u8 video_mapping;
	u8 color_format;
	u8 bpc;
	u8 bpp;
};

enum audio_format {
	AFMT_I2S = 0,
	AFMT_SPDIF = 1,
	AFMT_UNUSED,
};

struct dw_dp_audio {
	struct platform_device *pdev;
	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	struct extcon_dev *extcon;
	struct clk *i2s_clk;
	struct clk *spdif_clk;
	enum audio_format format;
	u8 channels;
	int id;
};

struct dw_dp_sdp {
	struct dp_sdp_header header;
	u8 db[32];
	unsigned long flags;
	int stream_id;
};

enum gpio_hpd_state {
	GPIO_STATE_IDLE,
	GPIO_STATE_PLUG,
	GPIO_STATE_UNPLUG,
};

struct dw_dp_hotplug {
	struct delayed_work state_work;
	enum gpio_hpd_state state;
	bool long_hpd;
	bool status;
};

struct dw_dp_compliance_data {
	struct drm_dp_phy_test_params phytest;
};

struct dw_dp_compliance {
	unsigned long test_type;
	struct dw_dp_compliance_data test_data;
	bool test_active;
};

struct dw_dp_ts {
	u32 average_bytes_per_tu;
	u32 average_bytes_per_tu_frac;
	u32 init_threshold;
};

struct dw_dp_port_caps {
	int max_hactive;
	int max_vactive;
	int max_pixel_clock;
};

struct dw_dp_chip_data {
	int chip_type;
	bool mst;
	int mst_port_num;
	int pixel_mode;
	struct dw_dp_port_caps caps[DPTX_MAX_STREAMS];
};

struct dw_dp;
struct dw_dp_mst_enc;
struct dw_dp_mst_conn {
	struct drm_connector connector;
	struct drm_dp_mst_port *port;
	struct dw_dp_mst_enc *mst_enc;
	struct dw_dp *dp;
	struct list_head head;

	u8 vsc_sdp_extension_for_colorimetry_supported;
};

struct dw_dp_mst_enc {
	struct drm_encoder encoder;
	struct dw_dp_video video;
	struct dw_dp_audio *audio;
	struct device_node *port_node;
	struct drm_bridge *next_bridge;

	DECLARE_BITMAP(sdp_reg_bank, SDP_REG_BANK_SIZE);

	struct dw_dp_mst_conn *mst_conn;
	struct dw_dp *dp;
	int stream_id;
	int fix_port_num;
	bool active;
};

struct dw_dp {
	const struct dw_dp_chip_data *chip_data;
	struct device *dev;
	struct regmap *regmap;
	struct phy *phy;
	struct clk *apb_clk;
	struct clk *aux_clk;
	struct clk *hclk;
	struct clk *hdcp_clk;
	struct reset_control *rstc;
	struct completion complete;
	struct completion hdcp_complete;
	int irq;
	int hpd_irq;
	int id;
	struct work_struct hpd_work;
	struct gpio_desc *hpd_gpio;
	bool force_hpd;
	bool usbdp_hpd;
	bool dynamic_pd_ctrl;
	struct dw_dp_hotplug hotplug;
	struct mutex irq_lock;

	struct drm_bridge bridge;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_dp_aux aux;
	struct drm_bridge *next_bridge;
	struct drm_panel *panel;

	struct dw_dp_link link;
	struct dw_dp_video video;
	struct dw_dp_audio *audio;
	struct dw_dp_compliance compliance;

	DECLARE_BITMAP(sdp_reg_bank, SDP_REG_BANK_SIZE);

	bool split_mode;
	bool dual_connector_split;
	bool left_display;

	struct dw_dp *left;
	struct dw_dp *right;

	struct drm_property *color_depth_property;
	struct drm_property *color_format_property;
	struct drm_property *color_depth_capacity;
	struct drm_property *color_format_capacity;
	struct drm_property *hdcp_state_property;
	struct drm_property *hdr_panel_metadata_property;
	struct drm_property_blob *hdr_panel_blob_ptr;

	struct rockchip_drm_sub_dev sub_dev;
	struct dw_dp_hdcp hdcp;
	int eotf_type;

	u8 pixel_mode;
	u32 max_link_rate;

	bool is_loader_protect;
	bool support_mst;
	bool is_mst;
	bool is_fix_port;
	int mst_port_num;
	int active_mst_links;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct dw_dp_mst_enc mst_enc[DPTX_MAX_STREAMS];
	struct list_head mst_conn_list;
	struct rockchip_dp_aux_client *aux_client;

	struct drm_info_list *debugfs_files;
	struct typec_mux_dev *mux;
};

struct dw_dp_state {
	struct drm_connector_state state;

	int bpc;
	int color_format;
};

struct hdcp_key_data_t {
	unsigned int signature;
	unsigned int length;
	unsigned int crc;
	unsigned int flags;
	unsigned char data[];
};

enum {
	DPTX_VM_RGB_6BIT,
	DPTX_VM_RGB_8BIT,
	DPTX_VM_RGB_10BIT,
	DPTX_VM_RGB_12BIT,
	DPTX_VM_RGB_16BIT,
	DPTX_VM_YCBCR444_8BIT,
	DPTX_VM_YCBCR444_10BIT,
	DPTX_VM_YCBCR444_12BIT,
	DPTX_VM_YCBCR444_16BIT,
	DPTX_VM_YCBCR422_8BIT,
	DPTX_VM_YCBCR422_10BIT,
	DPTX_VM_YCBCR422_12BIT,
	DPTX_VM_YCBCR422_16BIT,
	DPTX_VM_YCBCR420_8BIT,
	DPTX_VM_YCBCR420_10BIT,
	DPTX_VM_YCBCR420_12BIT,
	DPTX_VM_YCBCR420_16BIT,
};

enum {
	DPTX_MP_SINGLE_PIXEL,
	DPTX_MP_DUAL_PIXEL,
	DPTX_MP_QUAD_PIXEL,
};

enum {
	DPTX_SDP_VERTICAL_INTERVAL = BIT(0),
	DPTX_SDP_HORIZONTAL_INTERVAL = BIT(1),
};

enum {
	SOURCE_STATE_IDLE,
	SOURCE_STATE_UNPLUG,
	SOURCE_STATE_HPD_TIMEOUT = 4,
	SOURCE_STATE_PLUG = 7
};

enum {
	DPTX_PHY_PATTERN_NONE,
	DPTX_PHY_PATTERN_TPS_1,
	DPTX_PHY_PATTERN_TPS_2,
	DPTX_PHY_PATTERN_TPS_3,
	DPTX_PHY_PATTERN_TPS_4,
	DPTX_PHY_PATTERN_SERM,
	DPTX_PHY_PATTERN_PBRS7,
	DPTX_PHY_PATTERN_CUSTOM_80BIT,
	DPTX_PHY_PATTERN_CP2520_1,
	DPTX_PHY_PATTERN_CP2520_2,
};

static const unsigned int dw_dp_cable[] = {
	EXTCON_DISP_DP,
	EXTCON_NONE,
};

struct dw_dp_output_format {
	u32 bus_format;
	u32 color_format;
	u8 video_mapping;
	u8 bpc;
	u8 bpp;
};

static const struct dw_dp_output_format possible_output_fmts[] = {
	{ MEDIA_BUS_FMT_RGB101010_1X30, DRM_COLOR_FORMAT_RGB444,
	  DPTX_VM_RGB_10BIT, 10, 30 },
	{ MEDIA_BUS_FMT_RGB888_1X24, DRM_COLOR_FORMAT_RGB444,
	  DPTX_VM_RGB_8BIT, 8, 24 },
	{ MEDIA_BUS_FMT_YUV10_1X30, DRM_COLOR_FORMAT_YCBCR444,
	  DPTX_VM_YCBCR444_10BIT, 10, 30 },
	{ MEDIA_BUS_FMT_YUV8_1X24, DRM_COLOR_FORMAT_YCBCR444,
	  DPTX_VM_YCBCR444_8BIT, 8, 24},
	{ MEDIA_BUS_FMT_YUYV10_1X20, DRM_COLOR_FORMAT_YCBCR422,
	  DPTX_VM_YCBCR422_10BIT, 10, 20 },
	{ MEDIA_BUS_FMT_YUYV8_1X16, DRM_COLOR_FORMAT_YCBCR422,
	  DPTX_VM_YCBCR422_8BIT, 8, 16 },
	{ MEDIA_BUS_FMT_UYYVYY10_0_5X30, DRM_COLOR_FORMAT_YCBCR420,
	  DPTX_VM_YCBCR420_10BIT, 10, 15 },
	{ MEDIA_BUS_FMT_UYYVYY8_0_5X24, DRM_COLOR_FORMAT_YCBCR420,
	  DPTX_VM_YCBCR420_8BIT, 8, 12 },
	{ MEDIA_BUS_FMT_RGB666_1X24_CPADHI, DRM_COLOR_FORMAT_RGB444,
	  DPTX_VM_RGB_6BIT, 6, 18 },
};

static int dw_dp_hdcp_init_keys(struct dw_dp *dp)
{
	u32 val;
	int size;
	u8 hdcp_vendor_data[HDCP_DATA_SIZE + 1];
	void __iomem *base;
	struct arm_smccc_res res;
	struct hdcp_key_data_t *key_data;
	bool aes_encrypt;

	regmap_read(dp->regmap, DPTX_HDCPREG_RMLSTS, &val);
	if (FIELD_GET(IDPK_DATA_INDEX, val) == 40) {
		dev_info(dp->dev, "dpk keys already write\n");
		return 0;
	}

	size = rk_vendor_read(DP_HDCP1X_ID, hdcp_vendor_data, HDCP_DATA_SIZE);
	if (size < (HDCP_KEY_SIZE + HDCP_KEY_SEED_SIZE))  {
		dev_info(dp->dev, "HDCP key read error, size: %d\n", size);
		return -EINVAL;
	}

	key_data = (struct hdcp_key_data_t *)hdcp_vendor_data;
	if ((key_data->signature != HDCP_SIG_MAGIC) || !(key_data->flags & HDCP_FLG_AES))
		aes_encrypt = false;
	else
		aes_encrypt = true;

	base = sip_hdcp_request_share_memory(dp->id ? DP_TX1 : DP_TX0);
	if (!base)
		return -ENOMEM;

	memcpy_toio(base, hdcp_vendor_data, size);

	res = sip_hdcp_config(HDCP_FUNC_KEY_LOAD, dp->id ? DP_TX1 : DP_TX0, !aes_encrypt);
	if (IS_SIP_ERROR(res.a0)) {
		dev_err(dp->dev, "load hdcp key failed\n");
		return -EBUSY;
	}

	return 0;
}

static int dw_dp_hdcp_rng_init(struct dw_dp *dp)
{
	u32 random_val;

	regmap_write(dp->regmap, DPTX_HDCPREG_ANCONF, OANBYPASS);
	get_random_bytes(&random_val, sizeof(u32));
	regmap_write(dp->regmap, DPTX_HDCPREG_AN0, random_val);
	get_random_bytes(&random_val, sizeof(u32));
	regmap_write(dp->regmap, DPTX_HDCPREG_AN1, random_val);

	return 0;
}

static int dw_dp_hw_hdcp_init(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_SOFT_RESET_CTRL, HDCP_MODULE_RESET,
			FIELD_PREP(HDCP_MODULE_RESET, 1));
	udelay(10);
	regmap_update_bits(dp->regmap, DPTX_SOFT_RESET_CTRL, HDCP_MODULE_RESET,
			FIELD_PREP(HDCP_MODULE_RESET, 0));

	regmap_update_bits(dp->regmap, DPTX_GENERAL_INTERRUPT_ENABLE,
			HDCP_EVENT_EN, FIELD_PREP(HDCP_EVENT_EN, 1));

	return 0;
}

static bool dw_dp_hdcp2_capable(struct dw_dp *dp)
{
	u8 rx_caps[3];
	int ret;

	ret = drm_dp_dpcd_read(&dp->aux, DP_HDCP_2_2_REG_RX_CAPS_OFFSET,
			       rx_caps, HDCP_2_2_RXCAPS_LEN);
	if (ret != HDCP_2_2_RXCAPS_LEN) {
		dev_err(dp->dev, "get hdcp2 capable failed:%d\n", ret);
		return false;
	}

	if (rx_caps[0] == HDCP_2_2_RX_CAPS_VERSION_VAL &&
	    HDCP_2_2_DP_HDCP_CAPABLE(rx_caps[2]))
		return true;

	return false;
}

static int _dw_dp_hdcp2_disable(struct dw_dp *dp)
{
	struct dw_dp_hdcp *hdcp = &dp->hdcp;

	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, ENABLE_HDCP, 0);
	clk_disable_unprepare(dp->hdcp_clk);

	hdcp->status = HDCP_TX_NONE;

	dp->hdcp.hdcp2_encrypted = false;

	return 0;
}

static int dw_dp_hdcp2_auth_check(struct dw_dp *dp)
{
	u32 val;
	int ret;

	ret = regmap_read_poll_timeout(dp->regmap, DPTX_HDCPOBS, val,
				       FIELD_GET(HDCP22_BOOTED, val), 1000, 1000000);
	if (ret) {
		dev_err(dp->dev, "wait HDCP2 controller booted timeout\n");
		return ret;
	}

	ret = regmap_read_poll_timeout(dp->regmap, DPTX_HDCPOBS, val,
				       FIELD_GET(HDCP22_CAPABLE_SINK
						 | HDCP22_SINK_CAP_CHECK_COMPLETE, val),
				       1000, 1000000);
	if (ret) {
		dev_err(dp->dev, "sink not support HDCP2\n");
		return ret;
	}

	ret = regmap_read_poll_timeout(dp->regmap, DPTX_HDCPOBS, val,
				       FIELD_GET(HDCP22_AUTHENTICATION_SUCCESS, val),
				       1000, 2000000);
	if (ret) {
		dev_err(dp->dev, "wait hdcp22 controller auth timeout\n");
		return ret;
	}

	dp->hdcp.hdcp2_encrypted = true;

	dev_info(dp->dev, "HDCP2 authentication succeed\n");

	return ret;
}

static int _dw_dp_hdcp2_enable(struct dw_dp *dp)
{
	struct dw_dp_hdcp *hdcp = &dp->hdcp;

	hdcp->status = HDCP_TX_2;

	clk_prepare_enable(dp->hdcp_clk);

	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, ENABLE_HDCP, ENABLE_HDCP);

	return dw_dp_hdcp2_auth_check(dp);
}

static bool dw_dp_hdcp_capable(struct dw_dp *dp)
{
	struct dw_dp_hdcp *hdcp = &dp->hdcp;
	int ret;
	u8 bcaps;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_AUX_HDCP_BCAPS, &bcaps);
	if (ret != 1) {
		dev_err(dp->dev, "get hdcp capable failed:%d\n", ret);
		return false;
	}
	hdcp->is_repeater = (bcaps & DP_BCAPS_REPEATER_PRESENT) ? true : false;

	return bcaps & DP_BCAPS_HDCP_CAPABLE;
}

static int _dw_dp_hdcp_disable(struct dw_dp *dp)
{
	struct dw_dp_hdcp *hdcp = &dp->hdcp;

	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, ENABLE_HDCP | ENABLE_HDCP_13, 0);

	hdcp->status = HDCP_TX_NONE;

	dp->hdcp.hdcp_encrypted = false;

	return 0;
}

static int _dw_dp_hdcp_enable(struct dw_dp *dp)
{
	unsigned long timeout;
	int ret;
	u8 rev;
	struct dw_dp_hdcp *hdcp = &dp->hdcp;

	timeout = msecs_to_jiffies(hdcp->is_repeater ? 5200 : 1000);
	hdcp->status = HDCP_TX_1;

	dw_dp_hdcp_rng_init(dp);

	ret = dw_dp_hdcp_init_keys(dp);
	if (ret)
		return ret;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_DPCD_REV, &rev);
	if (ret < 0)
		return ret;

	if (rev > DP_DPCD_REV_12)
		regmap_update_bits(dp->regmap, DPTX_HDCPCFG, DPCD12PLUS, DPCD12PLUS);

	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, ENABLE_HDCP | ENABLE_HDCP_13,
			   ENABLE_HDCP | ENABLE_HDCP_13);

	ret = wait_for_completion_timeout(&dp->hdcp_complete, timeout);
	if (!ret) {
		dev_err(dp->dev, "HDCP authentication timeout\n");
		return -ETIMEDOUT;
	}

	hdcp->hdcp_encrypted = true;

	return 0;
}

static int dw_dp_hdcp_enable(struct dw_dp *dp, u8 content_type)
{
	int ret = -EINVAL;

	dp->hdcp.check_link_interval = DRM_HDCP_CHECK_PERIOD_MS;
	mutex_lock(&dp->hdcp.mutex);
	sip_hdcp_config(HDCP_FUNC_ENCRYPT_MODE, dp->id ? DP_TX1 : DP_TX0, 0x0);
	dw_dp_hw_hdcp_init(dp);
	if (dw_dp_hdcp2_capable(dp)) {
		ret = _dw_dp_hdcp2_enable(dp);
		if (!ret)
			dp->hdcp.check_link_interval = DRM_HDCP2_CHECK_PERIOD_MS;
		else
			_dw_dp_hdcp2_disable(dp);
	}

	if (ret && dw_dp_hdcp_capable(dp) && content_type != DRM_MODE_HDCP_CONTENT_TYPE1) {
		ret = _dw_dp_hdcp_enable(dp);
		if (!ret)
			dp->hdcp.check_link_interval = DRM_HDCP_CHECK_PERIOD_MS;
		else
			_dw_dp_hdcp_disable(dp);
	}

	if (ret)
		goto out;

	dp->hdcp.hdcp_content_type = content_type;
	dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_ENABLED;
	schedule_work(&dp->hdcp.prop_work);
	schedule_delayed_work(&dp->hdcp.check_work, dp->hdcp.check_link_interval);

out:
	mutex_unlock(&dp->hdcp.mutex);
	return ret;
}

static int dw_dp_hdcp_disable(struct dw_dp *dp)
{
	int ret = 0;

	mutex_lock(&dp->hdcp.mutex);
	if (dp->hdcp.value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
		dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
		sip_hdcp_config(HDCP_FUNC_ENCRYPT_MODE, dp->id ? DP_TX1 : DP_TX0, 0x1);
		ret = _dw_dp_hdcp_disable(dp);
	}
	mutex_unlock(&dp->hdcp.mutex);
	cancel_delayed_work_sync(&dp->hdcp.check_work);

	return ret;
}

static int _dw_dp_hdcp_check_link(struct dw_dp *dp)
{
	u8 bstatus;
	int ret;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_AUX_HDCP_BSTATUS, &bstatus);
	if (ret < 0)
		return ret;

	if (bstatus & (DP_BSTATUS_LINK_FAILURE | DP_BSTATUS_REAUTH_REQ))
		return -EINVAL;

	return 0;
}

static int dw_dp_hdcp_check_link(struct dw_dp *dp)
{
	int ret = 0;

	mutex_lock(&dp->hdcp.mutex);

	if (dp->hdcp.value == DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		goto out;

	ret = _dw_dp_hdcp_check_link(dp);
	if (!ret)
		goto out;

	dev_info(dp->dev, "HDCP link failed, retrying authentication\n");

	if (dp->hdcp.status == HDCP_TX_2) {
		ret = _dw_dp_hdcp2_disable(dp);
		if (ret) {
			dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&dp->hdcp.prop_work);
			goto out;
		}

		ret = _dw_dp_hdcp2_enable(dp);
		if (ret) {
			dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&dp->hdcp.prop_work);
		}
	} else if (dp->hdcp.status == HDCP_TX_1) {
		ret = _dw_dp_hdcp_disable(dp);
		if (ret) {
			dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&dp->hdcp.prop_work);
			goto out;
		}

		ret = _dw_dp_hdcp_enable(dp);
		if (ret) {
			dp->hdcp.value = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&dp->hdcp.prop_work);
		}
	}

out:
	mutex_unlock(&dp->hdcp.mutex);
	return ret;
}

static void dw_dp_hdcp_check_work(struct work_struct *work)
{
	struct delayed_work *d_work = to_delayed_work(work);
	struct dw_dp_hdcp *hdcp =
		container_of(d_work, struct dw_dp_hdcp, check_work);
	struct dw_dp *dp =
		container_of(hdcp, struct dw_dp, hdcp);

	if (!dw_dp_hdcp_check_link(dp))
		schedule_delayed_work(&hdcp->check_work,
				      hdcp->check_link_interval);
}

static void dp_dp_hdcp_prop_work(struct work_struct *work)
{
	struct dw_dp_hdcp *hdcp =
		container_of(work, struct dw_dp_hdcp, prop_work);
	struct dw_dp *dp =
		container_of(hdcp, struct dw_dp, hdcp);
	struct drm_device *dev = dp->connector.dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	mutex_lock(&dp->hdcp.mutex);
	if (dp->hdcp.value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		drm_hdcp_update_content_protection(&dp->connector, dp->hdcp.value);
	mutex_unlock(&dp->hdcp.mutex);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}

static void dw_dp_hdcp_init(struct dw_dp *dp)
{
	INIT_DELAYED_WORK(&dp->hdcp.check_work, dw_dp_hdcp_check_work);
	INIT_WORK(&dp->hdcp.prop_work, dp_dp_hdcp_prop_work);
	mutex_init(&dp->hdcp.mutex);
}

static void dw_dp_handle_hdcp_event(struct dw_dp *dp)
{
	u32 value;

	mutex_lock(&dp->irq_lock);

	regmap_read(dp->regmap, DPTX_HDCPAPIINTSTAT, &value);

	if (value & KSVACCESSINT)
		dev_err(dp->dev, "Notify ksv access\n");

	if (value & AUXRESPDEFER7TIMES)
		dev_err_ratelimited(dp->dev,
				    "Aux received defer response continuously for 7 times\n");

	if (value & AUXRESPTIMEOUT)
		dev_err(dp->dev, "Aux did not receive a response and timedout\n");

	if (value & AUXRESPNACK7TIMES)
		dev_err_ratelimited(dp->dev,
				    "Aux received nack response continuously for 7 times\n");

	if (value & KSVSHA1CALCDONEINT)
		dev_info(dp->dev, "Notify SHA1 verification has been done\n");

	if (value & HDCP22_GPIOINT)
		dev_info(dp->dev, "A change in HDCP22 GPIO Output status\n");

	if (value & HDCP_FAILED)
		dev_err(dp->dev, " HDCP authentication process failed\n");

	if (value & HDCP_ENGAGED) {
		complete(&dp->hdcp_complete);
		dev_info(dp->dev, "HDCP authentication succeed\n");
	}

	regmap_write(dp->regmap, DPTX_HDCPAPIINTCLR, value);
	mutex_unlock(&dp->irq_lock);
}

static const struct drm_prop_enum_list color_depth_enum_list[] = {
	{ 0, "Automatic" },
	{ 6, "18bit" },
	{ 8, "24bit" },
	{ 10, "30bit" },
};

static const struct drm_prop_enum_list color_format_enum_list[] = {
	{ RK_IF_FORMAT_RGB, "rgb" },
	{ RK_IF_FORMAT_YCBCR444, "ycbcr444" },
	{ RK_IF_FORMAT_YCBCR422, "ycbcr422" },
	{ RK_IF_FORMAT_YCBCR420, "ycbcr420" },
	{ RK_IF_FORMAT_YCBCR_HQ, "ycbcr_high_subsampling" },
	{ RK_IF_FORMAT_YCBCR_LQ, "ycbcr_low_subsampling" },
};

static const struct dw_dp_output_format *dw_dp_get_output_format(u32 bus_format)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(possible_output_fmts); i++)
		if (possible_output_fmts[i].bus_format == bus_format)
			return &possible_output_fmts[i];

	return &possible_output_fmts[1];
}

static inline struct dw_dp *connector_to_dp(struct drm_connector *c)
{
	return container_of(c, struct dw_dp, connector);
}

static inline struct dw_dp *encoder_to_dp(struct drm_encoder *e)
{
	return container_of(e, struct dw_dp, encoder);
}

static inline struct dw_dp *bridge_to_dp(struct drm_bridge *b)
{
	return container_of(b, struct dw_dp, bridge);
}

static inline struct dw_dp_state *connector_to_dp_state(struct drm_connector_state *cstate)
{
	return container_of(cstate, struct dw_dp_state, state);
}

static int dw_dp_match_by_id(struct device *dev, const void *data)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	const unsigned int *id = data;

	return dp->id == *id;
}

static struct dw_dp *dw_dp_find_by_id(struct device_driver *drv,
				      unsigned int id)
{
	struct device *dev;

	dev = driver_find_device(drv, NULL, &id, dw_dp_match_by_id);
	if (!dev)
		return NULL;

	return dev_get_drvdata(dev);
}

static void dw_dp_phy_set_pattern(struct dw_dp *dp, u32 pattern)
{
	regmap_update_bits(dp->regmap, DPTX_PHYIF_CTRL, TPS_SEL,
			   FIELD_PREP(TPS_SEL, pattern));
}

static void dw_dp_phy_xmit_enable(struct dw_dp *dp, u32 lanes)
{
	u32 xmit_enable;

	switch (lanes) {
	case 4:
	case 2:
	case 1:
		xmit_enable = GENMASK(lanes - 1, 0);
		break;
	case 0:
	default:
		xmit_enable = 0;
		break;
	}

	regmap_update_bits(dp->regmap, DPTX_PHYIF_CTRL, XMIT_ENABLE,
			   FIELD_PREP(XMIT_ENABLE, xmit_enable));
}

static bool dw_dp_bandwidth_ok(struct dw_dp *dp,
			       const struct drm_display_mode *mode, u32 bpp,
			       unsigned int lanes, unsigned int rate)
{
	u32 max_bw, req_bw;

	req_bw = mode->clock * bpp / 8;
	max_bw = lanes * rate;
	/*
	 * In MST mode, a MTP is 64 time slots, The first time slots is for
	 * MTP Header, only 63 time slots for payload, so when calculate the
	 * request bandwidth, req_bw = req_bw * 64 / 63;
	 */
	if (dp->is_mst)
		req_bw = DIV_ROUND_UP(req_bw * 64, 63);
	if (req_bw > max_bw)
		return false;

	return true;
}

static bool dw_dp_detect_no_power(struct dw_dp *dp)
{
	u32 value;
	int ret;

	if (dp->hpd_gpio)
		return gpiod_get_value_cansleep(dp->hpd_gpio);

	if (dp->usbdp_hpd)
		return dp->hotplug.status;

	if (dp->force_hpd)
		return true;

	ret = regmap_read_poll_timeout(dp->regmap, DPTX_HPD_STATUS, value,
				       FIELD_GET(HPD_STATE, value) != SOURCE_STATE_UNPLUG,
				       100, 3000);
	if (ret) {
		dev_err(dp->dev, "get hpd status timeout\n");
		return false;
	}

	return FIELD_GET(HPD_STATE, value) == SOURCE_STATE_PLUG;
}

static bool dw_dp_detect(struct dw_dp *dp)
{
	bool detected;

	pm_runtime_get_sync(dp->dev);

	detected = dw_dp_detect_no_power(dp);

	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);

	return detected;
}

static enum drm_connector_status
dw_dp_connector_detect(struct drm_connector *connector, bool force)
{
	struct dw_dp *dp = connector_to_dp(connector);

	if (dp->right && drm_bridge_detect(&dp->right->bridge) != connector_status_connected)
		return connector_status_disconnected;

	return drm_bridge_detect(&dp->bridge);
}

static void dw_dp_audio_handle_plugged_change(struct dw_dp_audio *audio, bool plugged)
{
	if (audio->plugged_cb && audio->codec_dev)
		audio->plugged_cb(audio->codec_dev, plugged);
}

static void dw_dp_connector_force(struct drm_connector *connector)
{
	struct dw_dp *dp = connector_to_dp(connector);

	if (connector->status == connector_status_connected) {
		extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, true);
		dw_dp_audio_handle_plugged_change(dp->audio, true);
	} else {
		extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, false);
		dw_dp_audio_handle_plugged_change(dp->audio, false);
	}
}

static void dw_dp_atomic_connector_reset(struct drm_connector *connector)
{
	struct dw_dp_state *dp_state = connector_to_dp_state(connector->state);

	if (connector->state) {
		__drm_atomic_helper_connector_destroy_state(connector->state);
		kfree(dp_state);
	}

	dp_state = kzalloc(sizeof(*dp_state), GFP_KERNEL);
	if (!dp_state)
		return;

	__drm_atomic_helper_connector_reset(connector, &dp_state->state);
	dp_state->bpc = 0;
	dp_state->color_format = RK_IF_FORMAT_RGB;
}

static struct drm_connector_state *
dw_dp_atomic_connector_duplicate_state(struct drm_connector *connector)
{
	struct dw_dp_state *cstate, *old_cstate;

	if (WARN_ON(!connector->state))
		return NULL;

	old_cstate = connector_to_dp_state(connector->state);
	cstate = kmalloc(sizeof(*cstate), GFP_KERNEL);
	if (!cstate)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector, &cstate->state);
	cstate->bpc = old_cstate->bpc;
	cstate->color_format = old_cstate->color_format;

	return &cstate->state;
}

static void dw_dp_atomic_connector_destroy_state(struct drm_connector *connector,
						 struct drm_connector_state *state)
{
	struct dw_dp_state *cstate = connector_to_dp_state(state);

	__drm_atomic_helper_connector_destroy_state(&cstate->state);
	kfree(cstate);
}

static int dw_dp_atomic_connector_get_property(struct drm_connector *connector,
					       const struct drm_connector_state *state,
					       struct drm_property *property,
					       uint64_t *val)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct dw_dp_state *dp_state = connector_to_dp_state((struct drm_connector_state *)state);
	struct drm_display_info *info = &connector->display_info;

	if (property == dp->color_depth_property) {
		*val = dp_state->bpc;
		return 0;
	} else if (property == dp->color_format_property) {
		*val = dp_state->color_format;
		return 0;
	} else if (property == dp->color_depth_capacity) {
		*val = BIT(RK_IF_DEPTH_8);
		switch (info->bpc) {
		case 16:
			fallthrough;
		case 12:
			fallthrough;
		case 10:
			*val |= BIT(RK_IF_DEPTH_10);
			fallthrough;
		case 8:
			*val |= BIT(RK_IF_DEPTH_8);
			fallthrough;
		case 6:
			*val |= BIT(RK_IF_DEPTH_6);
			fallthrough;
		default:
			break;
		}
		return 0;
	} else if (property == dp->color_format_capacity) {
		*val = info->color_formats;
		return 0;
	} else if (property == dp->hdcp_state_property) {
		if (dp->hdcp.hdcp2_encrypted)
			*val = RK_IF_HDCP_ENCRYPTED_LEVEL2;
		else if (dp->hdcp.hdcp_encrypted)
			*val = RK_IF_HDCP_ENCRYPTED_LEVEL1;
		else
			*val = RK_IF_HDCP_ENCRYPTED_NONE;
		return 0;
	}

	dev_err(dp->dev, "Unknown property [PROP:%d:%s]\n",
		  property->base.id, property->name);

	return 0;
}

static int dw_dp_atomic_connector_set_property(struct drm_connector *connector,
					       struct drm_connector_state *state,
					       struct drm_property *property,
					       uint64_t val)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct dw_dp_state *dp_state = connector_to_dp_state(state);

	if (property == dp->color_depth_property) {
		dp_state->bpc = val;
		return 0;
	} else if (property == dp->color_format_property) {
		dp_state->color_format = val;
		return 0;
	} else if (property == dp->color_depth_capacity) {
		return 0;
	} else if (property == dp->color_format_capacity) {
		return 0;
	} else if (property == dp->hdcp_state_property) {
		return 0;
	}

	dev_err(dp->dev, "Unknown property [PROP:%d:%s]\n",
		 property->base.id, property->name);

	return -EINVAL;
}

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_NO_GKI)
static int dw_dp_mst_info_dump(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct dw_dp *dp = node->info_ent->data;
	struct dw_dp_mst_conn *mst_conn;
	struct drm_property_blob *path_blob;
	int i;

	if (dp->mst_mgr.cbs) {
		drm_dp_mst_dump_topology(s, &dp->mst_mgr);

		seq_puts(s, "\n*** Connector path info ***\n");
		seq_puts(s, "connector name | connector path\n");
		list_for_each_entry(mst_conn, &dp->mst_conn_list, head) {
			path_blob = mst_conn->connector.path_blob_ptr;
			if (!path_blob)
				continue;

			seq_printf(s, "%-16s %s\n", mst_conn->connector.name,
				   (char *)path_blob->data);
		}
		seq_puts(s, "\n");
		if (dp->is_fix_port) {
			seq_puts(s, "\n*** Fix port info ***\n");
			seq_puts(s, "stream id | port num\n");

			for (i = 0; i < dp->mst_port_num; i++) {
				if (!dp->mst_enc[i].dp)
					continue;
				seq_printf(s, "%-16d %d\n", dp->mst_enc[i].stream_id,
					   dp->mst_enc[i].fix_port_num);
			}
		}
	}

	return 0;
}

static struct drm_info_list dw_dp_debugfs_files[] = {
	{ "dp_mst_info", dw_dp_mst_info_dump, 0, NULL },
};

static int dw_dp_connector_late_register(struct drm_connector *connector)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct drm_minor *minor = connector->dev->primary;
	int i;

	dp->debugfs_files = kmemdup(dw_dp_debugfs_files, sizeof(dw_dp_debugfs_files), GFP_KERNEL);
	if (!dp->debugfs_files)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(dw_dp_debugfs_files); i++)
		dp->debugfs_files[i].data = dp;

	drm_debugfs_create_files(dp->debugfs_files, ARRAY_SIZE(dw_dp_debugfs_files),
				 connector->debugfs_entry, minor);

	return 0;
}

static void dw_dp_connector_early_unregister(struct drm_connector *connector)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct drm_minor *minor = connector->dev->primary;

	drm_debugfs_remove_files(dp->debugfs_files, ARRAY_SIZE(dw_dp_debugfs_files), minor);
	kfree(dp->debugfs_files);
}
#endif

static const struct drm_connector_funcs dw_dp_connector_funcs = {
	.detect			= dw_dp_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.destroy		= drm_connector_cleanup,
	.force			= dw_dp_connector_force,
	.reset			= dw_dp_atomic_connector_reset,
	.atomic_duplicate_state	= dw_dp_atomic_connector_duplicate_state,
	.atomic_destroy_state	= dw_dp_atomic_connector_destroy_state,
	.atomic_get_property	= dw_dp_atomic_connector_get_property,
	.atomic_set_property	= dw_dp_atomic_connector_set_property,
#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_NO_GKI)
	.late_register		= dw_dp_connector_late_register,
	.early_unregister	= dw_dp_connector_early_unregister,
#endif
};

static int dw_dp_update_hdr_property(struct drm_connector *connector)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct drm_device *dev = connector->dev;
	const struct hdr_static_metadata *metadata =
		&connector->hdr_sink_metadata.hdmi_type1;
	size_t size = sizeof(*metadata);
	int ret;

	ret = drm_property_replace_global_blob(dev, &dp->hdr_panel_blob_ptr, size, metadata,
					       &connector->base, dp->hdr_panel_metadata_property);

	return ret;
}

static int dw_dp_connector_get_modes(struct drm_connector *connector)
{
	struct dw_dp *dp = connector_to_dp(connector);
	struct drm_display_info *di = &connector->display_info;
	struct edid *edid;
	int num_modes = 0;

	if (dp->right && dp->right->next_bridge) {
		struct drm_bridge *bridge = dp->right->next_bridge;

		if (bridge->ops & DRM_BRIDGE_OP_MODES) {
			if (!drm_bridge_get_modes(bridge, connector))
				return 0;
		}
	}

	if (dp->next_bridge)
		num_modes = drm_bridge_get_modes(dp->next_bridge, connector);

	if (dp->panel)
		num_modes = drm_panel_get_modes(dp->panel, connector);

	if (!num_modes) {
		edid = drm_bridge_get_edid(&dp->bridge, connector);
		if (edid) {
			drm_connector_update_edid_property(connector, edid);
			num_modes = drm_add_edid_modes(connector, edid);
			dw_dp_update_hdr_property(connector);
			kfree(edid);
		}
	}

	if (!di->color_formats)
		di->color_formats = DRM_COLOR_FORMAT_RGB444;

	if (!di->bpc)
		di->bpc = 8;

	if (num_modes > 0 && (dp->split_mode || dp->dual_connector_split)) {
		struct drm_display_mode *mode;

		di->width_mm *= 2;

		list_for_each_entry(mode, &connector->probed_modes, head)
			drm_mode_convert_to_split_mode(mode);
	}

	return num_modes;
}

static int dw_dp_hdcp_atomic_check(struct drm_connector *conn,
					struct drm_atomic_state *state)
{
	struct drm_connector_state *old_state, *new_state;
	struct drm_crtc_state *crtc_state;
	u64 old_cp, new_cp;

	old_state = drm_atomic_get_old_connector_state(state, conn);
	new_state = drm_atomic_get_new_connector_state(state, conn);
	old_cp = old_state->content_protection;
	new_cp = new_state->content_protection;

	if (old_state->hdcp_content_type != new_state->hdcp_content_type &&
	    new_cp != DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
		new_state->content_protection = DRM_MODE_CONTENT_PROTECTION_DESIRED;
		goto mode_changed;
	}

	if (!new_state->crtc) {
		if (old_cp == DRM_MODE_CONTENT_PROTECTION_ENABLED)
			new_state->content_protection = DRM_MODE_CONTENT_PROTECTION_DESIRED;
		return 0;
	}

	if (old_cp == new_cp ||
	    (old_cp == DRM_MODE_CONTENT_PROTECTION_DESIRED &&
	     new_cp == DRM_MODE_CONTENT_PROTECTION_ENABLED))
		return 0;

mode_changed:
	crtc_state = drm_atomic_get_new_crtc_state(state, new_state->crtc);
	crtc_state->mode_changed = true;

	return 0;
}

static bool dw_dp_hdr_metadata_equal(const struct drm_connector_state *old_state,
				     const struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;

	if (!old_blob || !new_blob)
		return old_blob == new_blob;

	if (old_blob->length != new_blob->length)
		return false;

	return !memcmp(old_blob->data, new_blob->data, old_blob->length);
}

static inline bool dw_dp_is_hdr_eotf(int eotf)
{
	return eotf > HDMI_EOTF_TRADITIONAL_GAMMA_SDR && eotf <= HDMI_EOTF_BT_2100_HLG;
}

static int dw_dp_connector_atomic_check(struct drm_connector *conn,
					struct drm_atomic_state *state)
{
	struct drm_connector_state *old_state, *new_state;
	struct dw_dp_state *dp_old_state, *dp_new_state;
	struct drm_crtc_state *crtc_state;
	struct dw_dp *dp = connector_to_dp(conn);
	int ret;

	old_state = drm_atomic_get_old_connector_state(state, conn);
	new_state = drm_atomic_get_new_connector_state(state, conn);
	dp_old_state = connector_to_dp_state(old_state);
	dp_new_state = connector_to_dp_state(new_state);

	dw_dp_hdcp_atomic_check(conn, state);

	if (!new_state->crtc)
		return 0;

	crtc_state = drm_atomic_get_new_crtc_state(state, new_state->crtc);

	if (!dw_dp_hdr_metadata_equal(old_state, new_state))
		crtc_state->mode_changed = true;

	if ((dp_new_state->bpc != 0) && (dp_new_state->bpc != 6) && (dp_new_state->bpc != 8) &&
	    (dp_new_state->bpc != 10)) {
		dev_err(dp->dev, "set invalid bpc:%d\n", dp_new_state->bpc);
		return -EINVAL;
	}

	if ((dp_new_state->color_format < RK_IF_FORMAT_RGB) ||
	    (dp_new_state->color_format > RK_IF_FORMAT_YCBCR_LQ)) {
		dev_err(dp->dev, "set invalid color format:%d\n", dp_new_state->color_format);
		return -EINVAL;
	}

	if ((dp_old_state->bpc != dp_new_state->bpc) ||
	    (dp_old_state->color_format != dp_new_state->color_format)) {
		if ((dp_old_state->bpc == 0) && (dp_new_state->bpc == 0))
			dev_info(dp->dev, "still auto set color mode\n");
		else
			crtc_state->mode_changed = true;
	}

	if (dp->mst_mgr.cbs) {
		ret = drm_dp_mst_root_conn_atomic_check(new_state, &dp->mst_mgr);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct drm_connector_helper_funcs dw_dp_connector_helper_funcs = {
	.get_modes = dw_dp_connector_get_modes,
	.atomic_check = dw_dp_connector_atomic_check,
};

static void dw_dp_link_caps_reset(struct drm_dp_link_caps *caps)
{
	caps->enhanced_framing = false;
	caps->tps3_supported = false;
	caps->tps4_supported = false;
	caps->fast_training = false;
	caps->channel_coding = false;
}

static void dw_dp_link_reset(struct dw_dp_link *link)
{
	link->vsc_sdp_extension_for_colorimetry_supported = 0;
	link->sink_count = 0;
	link->revision = 0;

	dw_dp_link_caps_reset(&link->caps);
	memset(link->dpcd, 0, sizeof(link->dpcd));

	link->rate = 0;
	link->lanes = 0;
	link->max_rate = 0;
}

static int dw_dp_link_power_up(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 value;
	int ret;

	if (link->revision < 0x11)
		return 0;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_SET_POWER, &value);
	if (ret < 0)
		return ret;

	value &= ~DP_SET_POWER_MASK;
	value |= DP_SET_POWER_D0;

	ret = drm_dp_dpcd_writeb(&dp->aux, DP_SET_POWER, value);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	return 0;
}

static int dw_dp_link_power_down(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 value;
	int ret;

	if (link->revision < 0x11)
		return 0;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_SET_POWER, &value);
	if (ret < 0)
		return ret;

	value &= ~DP_SET_POWER_MASK;
	value |= DP_SET_POWER_D3;

	ret = drm_dp_dpcd_writeb(&dp->aux, DP_SET_POWER, value);
	if (ret < 0)
		return ret;

	return 0;
}

static bool dw_dp_has_sink_count(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				 const struct drm_dp_desc *desc)
{
	return dpcd[DP_DPCD_REV] >= DP_DPCD_REV_11 &&
	       dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT &&
	       !drm_dp_has_quirk(desc, DP_DPCD_QUIRK_NO_SINK_COUNT);
}

static int dw_dp_link_probe(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 dpcd;
	int ret;

	dw_dp_link_reset(link);

	ret = drm_dp_read_dpcd_caps(&dp->aux, link->dpcd);
	if (ret < 0)
		return ret;

	drm_dp_read_desc(&dp->aux, &link->desc, drm_dp_is_branch(link->dpcd));

	if (dw_dp_has_sink_count(link->dpcd, &link->desc)) {
		ret = drm_dp_read_sink_count(&dp->aux);
		if (ret < 0)
			return ret;

		link->sink_count = ret;

		/* Dongle connected, but no display */
		if (!link->sink_count)
			return -ENODEV;
	}

	link->sink_support_mst = drm_dp_read_mst_cap(&dp->aux, dp->link.dpcd);

	ret = drm_dp_dpcd_readb(&dp->aux, DP_DPRX_FEATURE_ENUMERATION_LIST,
				&dpcd);
	if (ret < 0)
		return ret;

	link->vsc_sdp_extension_for_colorimetry_supported =
			!!(dpcd & DP_VSC_SDP_EXT_FOR_COLORIMETRY_SUPPORTED);

	link->revision = link->dpcd[DP_DPCD_REV];
	link->max_rate = min_t(u32, min(dp->max_link_rate, dp->phy->attrs.max_link_rate * 100),
			 drm_dp_max_link_rate(link->dpcd));
	link->lanes = min_t(u8, phy_get_bus_width(dp->phy),
		      drm_dp_max_lane_count(link->dpcd));
	link->rate = link->max_rate;

	link->caps.enhanced_framing = drm_dp_enhanced_frame_cap(link->dpcd);
	link->caps.tps3_supported = drm_dp_tps3_supported(link->dpcd);
	link->caps.tps4_supported = drm_dp_tps4_supported(link->dpcd);
	link->caps.fast_training = drm_dp_fast_training_cap(link->dpcd);
	link->caps.channel_coding = drm_dp_channel_coding_supported(link->dpcd);
	link->caps.ssc = !!(link->dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5);

	return 0;
}

static int dw_dp_phy_update_vs_emph(struct dw_dp *dp, unsigned int rate, unsigned int lanes,
				    struct drm_dp_link_train_set *train_set)
{
	union phy_configure_opts phy_cfg;
	unsigned int *vs, *pe;
	u8 buf[4];
	int i, ret;

	vs = train_set->voltage_swing;
	pe = train_set->pre_emphasis;

	for (i = 0; i < lanes; i++) {
		phy_cfg.dp.voltage[i] = vs[i];
		phy_cfg.dp.pre[i] = pe[i];
	}

	phy_cfg.dp.lanes = lanes;
	phy_cfg.dp.link_rate = rate / 100;
	phy_cfg.dp.set_lanes = false;
	phy_cfg.dp.set_rate = false;
	phy_cfg.dp.set_voltages = true;

	ret = phy_configure(dp->phy, &phy_cfg);
	if (ret)
		return ret;

	for (i = 0; i < lanes; i++) {
		buf[i] = (vs[i] << DP_TRAIN_VOLTAGE_SWING_SHIFT) |
			 (pe[i] << DP_TRAIN_PRE_EMPHASIS_SHIFT);
		if (train_set->voltage_max_reached[i])
			buf[i] |= DP_TRAIN_MAX_SWING_REACHED;
		if (train_set->pre_max_reached[i])
			buf[i] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
	}

	ret = drm_dp_dpcd_write(&dp->aux, DP_TRAINING_LANE0_SET, buf, lanes);
	if (ret < 0)
		return ret;

	return 0;
}

static int dw_dp_link_train_update_vs_emph(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	struct drm_dp_link_train_set *request = &link->train.request;

	return dw_dp_phy_update_vs_emph(dp, dp->link.rate, dp->link.lanes, request);
}

static int dw_dp_phy_configure(struct dw_dp *dp, unsigned int rate,
			       unsigned int lanes, bool ssc)
{
	union phy_configure_opts phy_cfg;
	int ret;

	/* Move PHY to P3 */
	regmap_update_bits(dp->regmap, DPTX_PHYIF_CTRL, PHY_POWERDOWN,
			   FIELD_PREP(PHY_POWERDOWN, 0x3));

	phy_cfg.dp.lanes = lanes;
	phy_cfg.dp.link_rate = rate / 100;
	phy_cfg.dp.ssc = ssc;
	phy_cfg.dp.set_lanes = true;
	phy_cfg.dp.set_rate = true;
	phy_cfg.dp.set_voltages = false;
	ret = phy_configure(dp->phy, &phy_cfg);
	if (ret)
		return ret;

	regmap_update_bits(dp->regmap, DPTX_PHYIF_CTRL, PHY_LANES,
			   FIELD_PREP(PHY_LANES, lanes / 2));

	/* Move PHY to P0 */
	regmap_update_bits(dp->regmap, DPTX_PHYIF_CTRL, PHY_POWERDOWN,
			   FIELD_PREP(PHY_POWERDOWN, 0x0));

	dw_dp_phy_xmit_enable(dp, lanes);

	return 0;
}

static int dw_dp_link_configure(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 buf[2];
	int ret;

	ret = dw_dp_phy_configure(dp, link->rate, link->lanes, link->caps.ssc);
	if (ret)
		return ret;
	buf[0] = drm_dp_link_rate_to_bw_code(link->rate);
	buf[1] = link->lanes;

	if (link->caps.enhanced_framing) {
		buf[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN;
		regmap_update_bits(dp->regmap, DPTX_CCTL, ENHANCE_FRAMING_EN,
				   FIELD_PREP(ENHANCE_FRAMING_EN, 1));
	} else {
		regmap_update_bits(dp->regmap, DPTX_CCTL, ENHANCE_FRAMING_EN,
				   FIELD_PREP(ENHANCE_FRAMING_EN, 0));
	}

	ret = drm_dp_dpcd_write(&dp->aux, DP_LINK_BW_SET, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	buf[0] = link->caps.ssc ? DP_SPREAD_AMP_0_5 : 0;
	buf[1] = link->caps.channel_coding ? DP_SET_ANSI_8B10B : 0;

	ret = drm_dp_dpcd_write(&dp->aux, DP_DOWNSPREAD_CTRL, buf,
				sizeof(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static void dw_dp_link_train_init(struct drm_dp_link_train *train)
{
	struct drm_dp_link_train_set *request = &train->request;
	struct drm_dp_link_train_set *adjust = &train->adjust;
	unsigned int i;

	for (i = 0; i < 4; i++) {
		request->voltage_swing[i] = 0;
		adjust->voltage_swing[i] = 0;

		request->pre_emphasis[i] = 0;
		adjust->pre_emphasis[i] = 0;

		request->voltage_max_reached[i] = false;
		adjust->voltage_max_reached[i] = false;

		request->pre_max_reached[i] = false;
		adjust->pre_max_reached[i] = false;
	}

	train->clock_recovered = false;
	train->channel_equalized = false;
}

static bool dw_dp_link_train_valid(const struct drm_dp_link_train *train)
{
	return train->clock_recovered && train->channel_equalized;
}

static int dw_dp_link_train_set_pattern(struct dw_dp *dp, u32 pattern)
{
	u8 buf = 0;
	int ret;

	if (pattern && pattern != DP_TRAINING_PATTERN_4) {
		buf |= DP_LINK_SCRAMBLING_DISABLE;

		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 1));
	} else {
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 0));
	}

	switch (pattern) {
	case DP_TRAINING_PATTERN_DISABLE:
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_NONE);
		break;
	case DP_TRAINING_PATTERN_1:
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_1);
		break;
	case DP_TRAINING_PATTERN_2:
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_2);
		break;
	case DP_TRAINING_PATTERN_3:
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_3);
		break;
	case DP_TRAINING_PATTERN_4:
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_4);
		break;
	default:
		return -EINVAL;
	}

	ret = drm_dp_dpcd_writeb(&dp->aux, DP_TRAINING_PATTERN_SET,
				 buf | pattern);
	if (ret < 0)
		return ret;

	return 0;
}

static u8 dw_dp_voltage_max(u8 preemph)
{
	switch (preemph & DP_TRAIN_PRE_EMPHASIS_MASK) {
	case DP_TRAIN_PRE_EMPH_LEVEL_0:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
	case DP_TRAIN_PRE_EMPH_LEVEL_1:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
	case DP_TRAIN_PRE_EMPH_LEVEL_2:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_1;
	case DP_TRAIN_PRE_EMPH_LEVEL_3:
	default:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_0;
	}
}

static void dw_dp_link_get_adjustments(struct dw_dp_link *link,
				       u8 status[DP_LINK_STATUS_SIZE])
{
	struct drm_dp_link_train_set *adjust = &link->train.adjust;
	u8 v = 0;
	u8 p = 0;
	unsigned int i;

	for (i = 0; i < link->lanes; i++) {
		v = drm_dp_get_adjust_request_voltage(status, i);
		p = drm_dp_get_adjust_request_pre_emphasis(status, i);
		if (p >=  DP_TRAIN_PRE_EMPH_LEVEL_3) {
			adjust->pre_emphasis[i] = DP_TRAIN_PRE_EMPH_LEVEL_3 >>
						  DP_TRAIN_PRE_EMPHASIS_SHIFT;
			adjust->pre_max_reached[i] = true;
		} else {
			adjust->pre_emphasis[i] = p >> DP_TRAIN_PRE_EMPHASIS_SHIFT;
			adjust->pre_max_reached[i] = false;
		}
		v = min(v, dw_dp_voltage_max(p));
		if (v >= DP_TRAIN_VOLTAGE_SWING_LEVEL_3) {
			adjust->voltage_swing[i] = DP_TRAIN_VOLTAGE_SWING_LEVEL_3 >>
						   DP_TRAIN_VOLTAGE_SWING_SHIFT;
			adjust->voltage_max_reached[i] = true;
		} else {
			adjust->voltage_swing[i] = v >> DP_TRAIN_VOLTAGE_SWING_SHIFT;
			adjust->voltage_max_reached[i] = false;
		}
	}
}

static void dw_dp_link_train_adjust(struct drm_dp_link_train *train)
{
	struct drm_dp_link_train_set *request = &train->request;
	struct drm_dp_link_train_set *adjust = &train->adjust;
	unsigned int i;

	for (i = 0; i < 4; i++) {
		if (request->voltage_swing[i] != adjust->voltage_swing[i])
			request->voltage_swing[i] = adjust->voltage_swing[i];
		if (request->voltage_max_reached[i] != adjust->voltage_max_reached[i])
			request->voltage_max_reached[i] = adjust->voltage_max_reached[i];
	}

	for (i = 0; i < 4; i++) {
		if (request->pre_emphasis[i] != adjust->pre_emphasis[i])
			request->pre_emphasis[i] = adjust->pre_emphasis[i];
		if (request->pre_max_reached[i] != adjust->pre_max_reached[i])
			request->pre_max_reached[i] = adjust->pre_max_reached[i];
	}
}

static int dw_dp_link_clock_recovery(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 status[DP_LINK_STATUS_SIZE];
	unsigned int tries = 0;
	int ret;

	ret = dw_dp_link_train_set_pattern(dp, DP_TRAINING_PATTERN_1);
	if (ret)
		return ret;

	for (;;) {
		ret = dw_dp_link_train_update_vs_emph(dp);
		if (ret)
			return ret;

		drm_dp_link_train_clock_recovery_delay(&dp->aux, link->dpcd);

		ret = drm_dp_dpcd_read_link_status(&dp->aux, status);
		if (ret < 0) {
			dev_err(dp->dev, "failed to read link status: %d\n", ret);
			return ret;
		}

		if (drm_dp_clock_recovery_ok(status, link->lanes)) {
			link->train.clock_recovered = true;
			break;
		}

		dw_dp_link_get_adjustments(link, status);

		if (link->train.request.voltage_swing[0] ==
		    link->train.adjust.voltage_swing[0])
			tries++;
		else
			tries = 0;

		if (tries == 5)
			break;

		dw_dp_link_train_adjust(&link->train);
	}

	return 0;
}

static int dw_dp_link_channel_equalization(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 status[DP_LINK_STATUS_SIZE], pattern;
	unsigned int tries;
	int ret;

	if (link->caps.tps4_supported)
		pattern = DP_TRAINING_PATTERN_4;
	else if (link->caps.tps3_supported)
		pattern = DP_TRAINING_PATTERN_3;
	else
		pattern = DP_TRAINING_PATTERN_2;
	ret = dw_dp_link_train_set_pattern(dp, pattern);
	if (ret)
		return ret;

	for (tries = 1; tries < 5; tries++) {
		ret = dw_dp_link_train_update_vs_emph(dp);
		if (ret)
			return ret;

		drm_dp_link_train_channel_eq_delay(&dp->aux, link->dpcd);

		ret = drm_dp_dpcd_read_link_status(&dp->aux, status);
		if (ret < 0)
			return ret;

		if (!drm_dp_clock_recovery_ok(status, link->lanes)) {
			dev_err(dp->dev, "clock recovery lost while equalizing channel\n");
			link->train.clock_recovered = false;
			break;
		}

		if (drm_dp_channel_eq_ok(status, link->lanes)) {
			link->train.channel_equalized = true;
			break;
		}

		dw_dp_link_get_adjustments(link, status);
		dw_dp_link_train_adjust(&link->train);
	}

	return 0;
}

static int dw_dp_link_downgrade(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	struct dw_dp_video *video = &dp->video;

	switch (link->rate) {
	case 162000:
		return -EINVAL;
	case 270000:
		link->rate = 162000;
		break;
	case 540000:
		link->rate = 270000;
		break;
	case 810000:
		link->rate = 540000;
		break;
	}

	if (!dw_dp_bandwidth_ok(dp, &video->mode, video->bpp, link->lanes,
				link->rate))
		return -E2BIG;

	return 0;
}

static int dw_dp_link_train_full(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	int ret;

retry:
	dw_dp_link_train_init(&link->train);

	dev_info(dp->dev, "full-training link: %u lane%s at %u MHz\n",
		 link->lanes, (link->lanes > 1) ? "s" : "", link->rate / 100);

	ret = dw_dp_link_configure(dp);
	if (ret < 0) {
		dev_err(dp->dev, "failed to configure DP link: %d\n", ret);
		return ret;
	}

	ret = dw_dp_link_clock_recovery(dp);
	if (ret < 0) {
		dev_err(dp->dev, "clock recovery failed: %d\n", ret);
		goto out;
	}

	if (!link->train.clock_recovered) {
		dev_err(dp->dev, "clock recovery failed, downgrading link\n");

		ret = dw_dp_link_downgrade(dp);
		if (ret < 0)
			goto out;
		else
			goto retry;
	}

	dev_info(dp->dev, "clock recovery succeeded\n");

	ret = dw_dp_link_channel_equalization(dp);
	if (ret < 0) {
		dev_err(dp->dev, "channel equalization failed: %d\n", ret);
		goto out;
	}

	if (!link->train.channel_equalized) {
		dev_err(dp->dev, "channel equalization failed, downgrading link\n");

		ret = dw_dp_link_downgrade(dp);
		if (ret < 0)
			goto out;
		else
			goto retry;
	}

	dev_info(dp->dev, "channel equalization succeeded\n");

out:
	dw_dp_link_train_set_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
	return ret;
}

static int dw_dp_link_train_fast(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 status[DP_LINK_STATUS_SIZE], pattern;
	int ret;

	dw_dp_link_train_init(&link->train);

	dev_info(dp->dev, "fast-training link: %u lane%s at %u MHz\n",
		 link->lanes, (link->lanes > 1) ? "s" : "", link->rate / 100);

	ret = dw_dp_link_configure(dp);
	if (ret < 0) {
		dev_err(dp->dev, "failed to configure DP link: %d\n", ret);
		return ret;
	}

	ret = dw_dp_link_train_set_pattern(dp, DP_TRAINING_PATTERN_1);
	if (ret)
		goto out;

	usleep_range(500, 1000);

	if (link->caps.tps4_supported)
		pattern = DP_TRAINING_PATTERN_4;
	else if (link->caps.tps3_supported)
		pattern = DP_TRAINING_PATTERN_3;
	else
		pattern = DP_TRAINING_PATTERN_2;
	ret = dw_dp_link_train_set_pattern(dp, pattern);
	if (ret)
		goto out;

	usleep_range(500, 1000);

	ret = drm_dp_dpcd_read_link_status(&dp->aux, status);
	if (ret < 0) {
		dev_err(dp->dev, "failed to read link status: %d\n", ret);
		goto out;
	}

	if (!drm_dp_clock_recovery_ok(status, link->lanes)) {
		dev_err(dp->dev, "clock recovery failed\n");
		ret = -EIO;
		goto out;
	}

	if (!drm_dp_channel_eq_ok(status, link->lanes)) {
		dev_err(dp->dev, "channel equalization failed\n");
		ret = -EIO;
		goto out;
	}

out:
	dw_dp_link_train_set_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
	return ret;
}

static int dw_dp_link_train(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	int ret;

	if (link->caps.fast_training) {
		if (dw_dp_link_train_valid(&link->train)) {
			ret = dw_dp_link_train_fast(dp);
			if (ret < 0)
				dev_err(dp->dev,
					"fast link training failed: %d\n", ret);
			else
				return 0;
		}
	}

	ret = dw_dp_link_train_full(dp);
	if (ret < 0) {
		dev_err(dp->dev, "full link training failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int dw_dp_send_sdp(struct dw_dp *dp, struct dw_dp_sdp *sdp)
{
	const u8 *payload = sdp->db;
	u32 reg;
	int i, nr;

	nr = find_first_zero_bit(dp->sdp_reg_bank, SDP_REG_BANK_SIZE);
	if (nr < SDP_REG_BANK_SIZE)
		set_bit(nr, dp->sdp_reg_bank);
	else
		return -EBUSY;

	reg = DPTX_SDP_REGISTER_BANK_N(sdp->stream_id) + nr * 9 * 4;

	/* SDP header */
	regmap_write(dp->regmap, reg, get_unaligned_le32(&sdp->header));

	/* SDP data payload */
	for (i = 1; i < 9; i++, payload += 4)
		regmap_write(dp->regmap, reg + i * 4,
			     FIELD_PREP(SDP_REGS, get_unaligned_le32(payload)));

	if (sdp->flags & DPTX_SDP_VERTICAL_INTERVAL)
		regmap_update_bits(dp->regmap, DPTX_SDP_VERTICAL_CTRL_N(sdp->stream_id),
				   EN_VERTICAL_SDP << nr,
				   EN_VERTICAL_SDP << nr);

	if (sdp->flags & DPTX_SDP_HORIZONTAL_INTERVAL)
		regmap_update_bits(dp->regmap, DPTX_SDP_HORIZONTAL_CTRL_N(sdp->stream_id),
				   EN_HORIZONTAL_SDP << nr,
				   EN_HORIZONTAL_SDP << nr);

	return 0;
}

static void dw_dp_vsc_sdp_pack(const struct drm_dp_vsc_sdp *vsc,
			       struct dw_dp_sdp *sdp)
{
	sdp->header.HB0 = 0;
	sdp->header.HB1 = DP_SDP_VSC;
	sdp->header.HB2 = vsc->revision;
	sdp->header.HB3 = vsc->length;

	sdp->db[16] = (vsc->pixelformat & 0xf) << 4;
	sdp->db[16] |= vsc->colorimetry & 0xf;

	switch (vsc->bpc) {
	case 8:
		sdp->db[17] = 0x1;
		break;
	case 10:
		sdp->db[17] = 0x2;
		break;
	case 12:
		sdp->db[17] = 0x3;
		break;
	case 16:
		sdp->db[17] = 0x4;
		break;
	case 6:
	default:
		break;
	}

	if (vsc->dynamic_range == DP_DYNAMIC_RANGE_CTA)
		sdp->db[17] |= 0x80;

	sdp->db[18] = vsc->content_type & 0x7;

	sdp->flags |= DPTX_SDP_VERTICAL_INTERVAL;
}

static int dw_dp_send_vsc_sdp(struct dw_dp *dp, int stream_id)
{
	struct dw_dp_video *video = &dp->video;
	struct drm_dp_vsc_sdp vsc = {};
	struct dw_dp_sdp sdp = {};

	sdp.stream_id = stream_id;
	vsc.revision = 0x5;
	vsc.length = 0x13;

	switch (video->color_format) {
	case DRM_COLOR_FORMAT_YCBCR444:
		vsc.pixelformat = DP_PIXELFORMAT_YUV444;
		break;
	case DRM_COLOR_FORMAT_YCBCR420:
		vsc.pixelformat = DP_PIXELFORMAT_YUV420;
		break;
	case DRM_COLOR_FORMAT_YCBCR422:
		vsc.pixelformat = DP_PIXELFORMAT_YUV422;
		break;
	case DRM_COLOR_FORMAT_RGB444:
	default:
		vsc.pixelformat = DP_PIXELFORMAT_RGB;
		break;
	}

	if (video->color_format == DRM_COLOR_FORMAT_RGB444) {
		if (dw_dp_is_hdr_eotf(dp->eotf_type))
			vsc.colorimetry = DP_COLORIMETRY_BT2020_RGB;
		else
			vsc.colorimetry = DP_COLORIMETRY_DEFAULT;
		vsc.dynamic_range = DP_DYNAMIC_RANGE_VESA;
	} else {
		if (dw_dp_is_hdr_eotf(dp->eotf_type))
			vsc.colorimetry = DP_COLORIMETRY_BT2020_YCC;
		else
			vsc.colorimetry = DP_COLORIMETRY_BT709_YCC;
		vsc.dynamic_range = DP_DYNAMIC_RANGE_CTA;
	}

	vsc.bpc = video->bpc;
	vsc.content_type = DP_CONTENT_TYPE_NOT_DEFINED;

	dw_dp_vsc_sdp_pack(&vsc, &sdp);

	return dw_dp_send_sdp(dp, &sdp);
}

static ssize_t dw_dp_hdr_metadata_infoframe_sdp_pack(struct dw_dp *dp,
						     const struct hdmi_drm_infoframe *drm_infoframe,
						     struct dw_dp_sdp *sdp)
{
	const int infoframe_size = HDMI_INFOFRAME_HEADER_SIZE + HDMI_DRM_INFOFRAME_SIZE;
	unsigned char buf[HDMI_INFOFRAME_HEADER_SIZE + HDMI_DRM_INFOFRAME_SIZE];
	ssize_t len;

	memset(sdp, 0, sizeof(*sdp));

	len = hdmi_drm_infoframe_pack_only(drm_infoframe, buf, sizeof(buf));
	if (len < 0) {
		dev_err(dp->dev, "buffer size is smaller than hdr metadata infoframe\n");
		return -ENOSPC;
	}

	if (len != infoframe_size) {
		dev_err(dp->dev, "wrong static hdr metadata size\n");
		return -ENOSPC;
	}

	sdp->header.HB0 = 0;
	sdp->header.HB1 = drm_infoframe->type;
	sdp->header.HB2 = 0x1D;
	sdp->header.HB3 = (0x13 << 2);
	sdp->db[0] = drm_infoframe->version;
	sdp->db[1] = drm_infoframe->length;

	memcpy(&sdp->db[2], &buf[HDMI_INFOFRAME_HEADER_SIZE],
	       HDMI_DRM_INFOFRAME_SIZE);

	sdp->flags |= DPTX_SDP_VERTICAL_INTERVAL;

	return sizeof(struct dp_sdp_header) + 2 + HDMI_DRM_INFOFRAME_SIZE;
}

static int dw_dp_send_hdr_metadata_infoframe_sdp(struct dw_dp *dp, int stream_id)
{
	struct hdmi_drm_infoframe drm_infoframe = {};
	struct dw_dp_sdp sdp = {};
	struct drm_connector_state *conn_state;
	int ret;

	sdp.stream_id = stream_id;
	conn_state = dp->connector.state;

	ret = drm_hdmi_infoframe_set_hdr_metadata(&drm_infoframe, conn_state);
	if (ret) {
		dev_err(dp->dev, "couldn't set HDR metadata in infoframe\n");
		return ret;
	}

	dw_dp_hdr_metadata_infoframe_sdp_pack(dp, &drm_infoframe, &sdp);

	return dw_dp_send_sdp(dp, &sdp);
}

static int dw_dp_video_set_pixel_mode(struct dw_dp *dp, int stream_id, u8 pixel_mode)
{
	switch (pixel_mode) {
	case DPTX_MP_SINGLE_PIXEL:
	case DPTX_MP_DUAL_PIXEL:
	case DPTX_MP_QUAD_PIXEL:
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(dp->regmap, DPTX_VSAMPLE_CTRL_N(stream_id), PIXEL_MODE_SELECT,
			   FIELD_PREP(PIXEL_MODE_SELECT, pixel_mode));

	return 0;
}

static bool dw_dp_video_need_vsc_sdp(struct dw_dp *dp, int stream_id)
{
	struct dw_dp_link *link = &dp->link;
	struct dw_dp_video *video = &dp->video;

	if (dp->is_mst) {
		struct dw_dp_mst_conn *mst_conn = dp->mst_enc[stream_id].mst_conn;

		if (!mst_conn || !mst_conn->vsc_sdp_extension_for_colorimetry_supported)
			return false;

	} else {
		if (!link->vsc_sdp_extension_for_colorimetry_supported)
			return false;
	}

	if (video->color_format == DRM_COLOR_FORMAT_YCBCR420)
		return true;

	if (dw_dp_is_hdr_eotf(dp->eotf_type))
		return true;

	return false;
}

static int dw_dp_video_set_msa(struct dw_dp *dp, struct drm_display_mode *mode, int stream_id,
			       u8 color_format, u8 bpc)
{
	u32 vstart = mode->crtc_vtotal - mode->crtc_vsync_start;
	u32 hstart = mode->crtc_htotal - mode->crtc_hsync_start;
	u16 misc = 0;

	if (dw_dp_video_need_vsc_sdp(dp, stream_id))
		misc |= DP_MSA_MISC_COLOR_VSC_SDP;

	switch (color_format) {
	case DRM_COLOR_FORMAT_RGB444:
		misc |= DP_MSA_MISC_COLOR_RGB;
		break;
	case DRM_COLOR_FORMAT_YCBCR444:
		misc |= DP_MSA_MISC_COLOR_YCBCR_444_BT709;
		break;
	case DRM_COLOR_FORMAT_YCBCR422:
		misc |= DP_MSA_MISC_COLOR_YCBCR_422_BT709;
		break;
	case DRM_COLOR_FORMAT_YCBCR420:
		break;
	default:
		return -EINVAL;
	}

	switch (bpc) {
	case 6:
		misc |= DP_MSA_MISC_6_BPC;
		break;
	case 8:
		misc |= DP_MSA_MISC_8_BPC;
		break;
	case 10:
		misc |= DP_MSA_MISC_10_BPC;
		break;
	case 12:
		misc |= DP_MSA_MISC_12_BPC;
		break;
	case 16:
		misc |= DP_MSA_MISC_16_BPC;
		break;
	default:
		return -EINVAL;
	}

	if ((mode->flags & DRM_MODE_FLAG_INTERLACE) && !(mode->vtotal % 2))
		misc |= DP_MSA_MISC_INTERLACE_VTOTAL_EVEN;

	regmap_write(dp->regmap, DPTX_VIDEO_MSA1_N(stream_id),
		     FIELD_PREP(VSTART, vstart) | FIELD_PREP(HSTART, hstart));
	regmap_write(dp->regmap, DPTX_VIDEO_MSA2_N(stream_id), FIELD_PREP(MISC0, misc));
	regmap_write(dp->regmap, DPTX_VIDEO_MSA3_N(stream_id), FIELD_PREP(MISC1, misc >> 8));

	return 0;
}

static void dw_dp_video_disable(struct dw_dp *dp, int stream_id)
{
	regmap_update_bits(dp->regmap, DPTX_VSAMPLE_CTRL_N(stream_id), VIDEO_STREAM_ENABLE,
			   FIELD_PREP(VIDEO_STREAM_ENABLE, 0));
}

static int dw_dp_video_ts_calculate(struct dw_dp *dp, struct dw_dp_video *video,
				    struct dw_dp_ts *ts)
{
	struct dw_dp_link *link = &dp->link;
	struct drm_display_mode *mode = &video->mode;
	u8 color_format = video->color_format;
	u8 bpp = video->bpp;
	u8 bpc = video->bpc;
	u8 pixel_mode = dp->pixel_mode;
	u32 peak_stream_bandwidth, link_bandwidth;
	u32 ts_calc;
	u32 t1 = 0, t2 = 0, t3 = 0;
	u32 hblank = mode->crtc_htotal - mode->crtc_hdisplay;

	peak_stream_bandwidth = mode->crtc_clock * bpp / 8;
	link_bandwidth = (link->rate / 1000) * link->lanes;
	ts_calc = peak_stream_bandwidth * 64 / link_bandwidth;
	ts->average_bytes_per_tu = ts_calc / 1000;
	ts->average_bytes_per_tu_frac = ts_calc / 100 - ts->average_bytes_per_tu * 10;
	if (pixel_mode == DPTX_MP_SINGLE_PIXEL) {
		if (ts->average_bytes_per_tu < 6)
			ts->init_threshold = 32;
		else if (hblank <= 80 && color_format != DRM_COLOR_FORMAT_YCBCR420)
			ts->init_threshold = 12;
		else if (hblank <= 40 && color_format == DRM_COLOR_FORMAT_YCBCR420)
			ts->init_threshold = 3;
		else
			ts->init_threshold = 16;
	} else {
		switch (bpc) {
		case 6:
			t1 = (4 * 1000 / 9) * link->lanes;
			break;
		case 8:
			if (color_format == DRM_COLOR_FORMAT_YCBCR422) {
				t1 = (1000 / 2) * link->lanes;
			} else {
				if (pixel_mode == DPTX_MP_DUAL_PIXEL)
					t1 = (1000 / 3) * link->lanes;
				else
					t1 = (3000 / 16) * link->lanes;
			}
			break;
		case 10:
			if (color_format == DRM_COLOR_FORMAT_YCBCR422)
				t1 = (2000 / 5) * link->lanes;
			else
				t1 = (4000 / 15) * link->lanes;
			break;
		case 12:
			if (color_format == DRM_COLOR_FORMAT_YCBCR422) {
				if (pixel_mode == DPTX_MP_DUAL_PIXEL)
					t1 = (1000 / 6) * link->lanes;
				else
					t1 = (1000 / 3) * link->lanes;
			} else {
				t1 = (2000 / 9) * link->lanes;
			}
			break;
		case 16:
			if (color_format != DRM_COLOR_FORMAT_YCBCR422 &&
			    pixel_mode == DPTX_MP_DUAL_PIXEL)
				t1 = (1000 / 6) * link->lanes;
			else
				t1 = (1000 / 4) * link->lanes;
			break;
		default:
			return -EINVAL;
		}

		if (color_format == DRM_COLOR_FORMAT_YCBCR420)
			t2 = (link->rate / 4) * 1000 / (mode->crtc_clock / 2);
		else
			t2 = (link->rate / 4) * 1000 / mode->crtc_clock;

		if (ts->average_bytes_per_tu_frac)
			t3 = ts->average_bytes_per_tu + 1;
		else
			t3 = ts->average_bytes_per_tu;
		ts->init_threshold = t1 * t2 * t3 / (1000 * 1000);
		if (ts->init_threshold <= 16 || ts->average_bytes_per_tu < 10)
			ts->init_threshold = 40;
	}

	return 0;
}

static int dw_dp_video_mst_ts_calculate(struct dw_dp *dp, struct dw_dp_video *video,
					struct dw_dp_ts *ts)
{
	struct dw_dp_link *link = &dp->link;
	struct drm_display_mode *mode = &video->mode;
	u8 color_format = video->color_format;
	u32 peak_stream_bandwidth, link_bandwidth;
	u32 ts_calc;
	u32 t1 = 0, t2 = 0, t3 = 0;
	u32 slot_count = 0;
	u32 num_lanes_divisor = 0;
	u32 slot_count_adjust = 0;

	peak_stream_bandwidth = mode->crtc_clock * video->bpp / 8;
	link_bandwidth = (link->rate / 1000) * link->lanes;
	ts_calc = peak_stream_bandwidth * 64 / link_bandwidth;
	ts->average_bytes_per_tu = ts_calc / 1000;
	ts->average_bytes_per_tu_frac = (ts_calc - ts->average_bytes_per_tu * 1000) * 64 / 1000;

	switch (video->bpc) {
	case 6:
		if (color_format == DRM_COLOR_FORMAT_YCBCR422)
			return -EINVAL;
		t1 = 72000 / 36;
		break;
	case 8:
		if (color_format == DRM_COLOR_FORMAT_YCBCR422)
			t1 = 8000 / 4;
		else
			t1 = 16000 / 12;
		break;
	case 10:
		if (color_format == DRM_COLOR_FORMAT_YCBCR422)
			t1 = 32000 / 20;
		else
			t1 = 64000 / 60;
		break;
	case 12:
		if (color_format == DRM_COLOR_FORMAT_YCBCR422)
			t1 = 16000 / 20;
		else
			t1 = 32000 / 36;
		break;
	case 16:
		if (color_format == DRM_COLOR_FORMAT_YCBCR422)
			t1 = 4000 / 4;
		else
			t1 = 8000 / 12;
		break;
	default:
		return -EINVAL;
	}

	if (color_format == DRM_COLOR_FORMAT_YCBCR420)
		t2 = (link->rate / 4) * 1000 / (mode->crtc_clock / 2);
	else
		t2 = (link->rate / 4) * 1000 / mode->crtc_clock;

	if (ts->average_bytes_per_tu_frac)
		slot_count = ts->average_bytes_per_tu + 1;
	else
		slot_count = ts->average_bytes_per_tu;

	if (link->lanes == 1) {
		num_lanes_divisor = 4;
		slot_count_adjust = 3;
	} else if (link->lanes == 2) {
		num_lanes_divisor = 2;
		slot_count_adjust = 1;
	} else {
		num_lanes_divisor = 1;
		slot_count_adjust = 0;
	}

	t3 = (slot_count + slot_count_adjust) + 8 * num_lanes_divisor;
	ts->init_threshold = t1 * t2 * t3 / (1000 * 1000 * num_lanes_divisor);

	return 0;
}

static int dw_dp_video_enable(struct dw_dp *dp, struct dw_dp_video *video, int stream_id)
{
	struct dw_dp_link *link = &dp->link;
	struct drm_display_mode *mode = &video->mode;
	struct dw_dp_ts ts;
	u8 color_format = video->color_format;
	u8 bpc = video->bpc;
	u8 pixel_mode = dp->pixel_mode;
	u8 vic;
	u32 hactive, hblank, h_sync_width, h_front_porch;
	u32 vactive, vblank, v_sync_width, v_front_porch;
	u32 hblank_interval;
	u32 value;
	int ret;

	ret = dw_dp_video_set_pixel_mode(dp, stream_id, pixel_mode);
	if (ret)
		return ret;

	ret = dw_dp_video_set_msa(dp, mode, stream_id, color_format, bpc);
	if (ret)
		return ret;

	regmap_update_bits(dp->regmap, DPTX_VSAMPLE_CTRL_N(stream_id), VIDEO_MAPPING,
			   FIELD_PREP(VIDEO_MAPPING, video->video_mapping));

	/* Configure DPTX_VINPUT_POLARITY_CTRL register */
	value = 0;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		value |= FIELD_PREP(HSYNC_IN_POLARITY, 1);
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		value |= FIELD_PREP(VSYNC_IN_POLARITY, 1);
	regmap_write(dp->regmap, DPTX_VINPUT_POLARITY_CTRL_N(stream_id), value);

	/* Configure DPTX_VIDEO_CONFIG1 register */
	hactive = mode->crtc_hdisplay;
	hblank = mode->crtc_htotal - mode->crtc_hdisplay;
	value = FIELD_PREP(HACTIVE, hactive) | FIELD_PREP(HBLANK, hblank);
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		value |= FIELD_PREP(I_P, 1);
	vic = drm_match_cea_mode(mode);
	if (vic == 5 || vic == 6 || vic == 7 ||
	    vic == 10 || vic == 11 || vic == 20 ||
	    vic == 21 || vic == 22 || vic == 39 ||
	    vic == 25 || vic == 26 || vic == 40 ||
	    vic == 44 || vic == 45 || vic == 46 ||
	    vic == 50 || vic == 51 || vic == 54 ||
	    vic == 55 || vic == 58 || vic  == 59)
		value |= R_V_BLANK_IN_OSC;
	regmap_write(dp->regmap, DPTX_VIDEO_CONFIG1_N(stream_id), value);

	/* Configure DPTX_VIDEO_CONFIG2 register */
	vblank = mode->crtc_vtotal - mode->crtc_vdisplay;
	vactive = mode->crtc_vdisplay;
	regmap_write(dp->regmap, DPTX_VIDEO_CONFIG2_N(stream_id),
		     FIELD_PREP(VBLANK, vblank) | FIELD_PREP(VACTIVE, vactive));

	/* Configure DPTX_VIDEO_CONFIG3 register */
	h_sync_width = mode->crtc_hsync_end - mode->crtc_hsync_start;
	h_front_porch = mode->crtc_hsync_start - mode->crtc_hdisplay;
	regmap_write(dp->regmap, DPTX_VIDEO_CONFIG3_N(stream_id),
		     FIELD_PREP(H_SYNC_WIDTH, h_sync_width) |
		     FIELD_PREP(H_FRONT_PORCH, h_front_porch));

	/* Configure DPTX_VIDEO_CONFIG4 register */
	v_sync_width = mode->crtc_vsync_end - mode->crtc_vsync_start;
	v_front_porch = mode->crtc_vsync_start - mode->crtc_vdisplay;
	regmap_write(dp->regmap, DPTX_VIDEO_CONFIG4_N(stream_id),
		     FIELD_PREP(V_SYNC_WIDTH, v_sync_width) |
		     FIELD_PREP(V_FRONT_PORCH, v_front_porch));

	/* Configure DPTX_VIDEO_CONFIG5 register */
	if (dp->is_mst) {
		ret = dw_dp_video_mst_ts_calculate(dp, &dp->mst_enc[stream_id].video, &ts);
		if (ret)
			return ret;
		regmap_write(dp->regmap, DPTX_VIDEO_CONFIG5_N(stream_id),
			     FIELD_PREP(INIT_THRESHOLD_HI, ts.init_threshold >> 6) |
			     FIELD_PREP(MST_AVERAGE_BYTES_PER_TU_FRAC,
					ts.average_bytes_per_tu_frac) |
			     FIELD_PREP(INIT_THRESHOLD, ts.init_threshold) |
			     FIELD_PREP(AVERAGE_BYTES_PER_TU, ts.average_bytes_per_tu));
	} else {
		ret = dw_dp_video_ts_calculate(dp, &dp->video, &ts);
		if (ret)
			return ret;
		regmap_write(dp->regmap, DPTX_VIDEO_CONFIG5_N(stream_id),
			     FIELD_PREP(INIT_THRESHOLD_HI, ts.init_threshold >> 6) |
			     FIELD_PREP(AVERAGE_BYTES_PER_TU_FRAC, ts.average_bytes_per_tu_frac) |
			     FIELD_PREP(INIT_THRESHOLD, ts.init_threshold) |
			     FIELD_PREP(AVERAGE_BYTES_PER_TU, ts.average_bytes_per_tu));
	}

	/* Configure DPTX_VIDEO_HBLANK_INTERVAL register */
	if (dp->is_mst)
		hblank_interval = hblank * ts.average_bytes_per_tu * (link->rate / 4) / 16 /
				  mode->crtc_clock;
	else
		hblank_interval = hblank * (link->rate / 4) / mode->crtc_clock;
	regmap_write(dp->regmap, DPTX_VIDEO_HBLANK_INTERVAL_N(stream_id),
		     FIELD_PREP(HBLANK_INTERVAL_EN, 1) |
		     FIELD_PREP(HBLANK_INTERVAL, hblank_interval));

	/* Video stream enable */
	regmap_update_bits(dp->regmap, DPTX_VSAMPLE_CTRL_N(stream_id), VIDEO_STREAM_ENABLE,
			   FIELD_PREP(VIDEO_STREAM_ENABLE, 1));

	if (dw_dp_video_need_vsc_sdp(dp, stream_id))
		dw_dp_send_vsc_sdp(dp, stream_id);

	if (dw_dp_is_hdr_eotf(dp->eotf_type))
		dw_dp_send_hdr_metadata_infoframe_sdp(dp, stream_id);

	return 0;
}

static void dw_dp_gpio_hpd_state_work(struct work_struct *work)
{
	struct dw_dp_hotplug *hotplug = container_of(to_delayed_work(work), struct dw_dp_hotplug,
						     state_work);
	struct dw_dp *dp = container_of(hotplug, struct dw_dp, hotplug);

	mutex_lock(&dp->irq_lock);
	if (hotplug->state == GPIO_STATE_UNPLUG) {
		dev_dbg(dp->dev, "hpd state unplug to idle\n");
		dp->hotplug.long_hpd = true;
		dp->hotplug.status = false;
		dp->hotplug.state = GPIO_STATE_IDLE;
		schedule_work(&dp->hpd_work);
	}
	mutex_unlock(&dp->irq_lock);
}

static irqreturn_t dw_dp_hpd_irq_handler(int irq, void *arg)
{
	struct dw_dp *dp = arg;
	bool hpd = dw_dp_detect(dp);

	dev_dbg(dp->dev, "trigger gpio to %s\n", hpd ? "high" : "low");
	mutex_lock(&dp->irq_lock);
	if (dp->hotplug.state == GPIO_STATE_IDLE) {
		if (hpd) {
			dev_dbg(dp->dev, "hpd state idle to plug\n");
			dp->hotplug.long_hpd = true;
			dp->hotplug.status = hpd;
			dp->hotplug.state = GPIO_STATE_PLUG;
		}
	} else if (dp->hotplug.state == GPIO_STATE_PLUG) {
		if (!hpd) {
			dev_dbg(dp->dev, "hpd state plug to unplug\n");
			dp->hotplug.state = GPIO_STATE_UNPLUG;
			schedule_delayed_work(&dp->hotplug.state_work, msecs_to_jiffies(2));
		}
	} else if (dp->hotplug.state == GPIO_STATE_UNPLUG) {
		if (hpd) {
			dev_dbg(dp->dev, "hpd state unplug to plug\n");
			cancel_delayed_work_sync(&dp->hotplug.state_work);
			dp->hotplug.long_hpd = false;
			dp->hotplug.status = hpd;
			dp->hotplug.state = GPIO_STATE_PLUG;
		}
	}

	if (hpd)
		schedule_work(&dp->hpd_work);

	mutex_unlock(&dp->irq_lock);


	return IRQ_HANDLED;
}

static void dw_dp_hpd_init(struct dw_dp *dp)
{
	dp->hotplug.status = dw_dp_detect_no_power(dp);
	if (dp->hpd_gpio && dp->hotplug.status) {
		dp->hotplug.state = GPIO_STATE_PLUG;
		dp->hotplug.long_hpd = true;
	}

	if (dp->hpd_gpio || dp->force_hpd || dp->usbdp_hpd) {
		regmap_update_bits(dp->regmap, DPTX_CCTL, FORCE_HPD,
				   FIELD_PREP(FORCE_HPD, 1));
		return;
	}

	/* Enable all HPD interrupts */
	regmap_update_bits(dp->regmap, DPTX_HPD_INTERRUPT_ENABLE,
			   HPD_UNPLUG_EN | HPD_PLUG_EN | HPD_IRQ_EN,
			   FIELD_PREP(HPD_UNPLUG_EN, 1) |
			   FIELD_PREP(HPD_PLUG_EN, 1) |
			   FIELD_PREP(HPD_IRQ_EN, 1));

	/* Enable all top-level interrupts */
	regmap_update_bits(dp->regmap, DPTX_GENERAL_INTERRUPT_ENABLE,
			   HPD_EVENT_EN, FIELD_PREP(HPD_EVENT_EN, 1));
}

static void dw_dp_aux_init(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_GENERAL_INTERRUPT_ENABLE,
			   AUX_REPLY_EVENT_EN,
			   FIELD_PREP(AUX_REPLY_EVENT_EN, 1));
}

static void dw_dp_init(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_CCTL, DEFAULT_FAST_LINK_TRAIN_EN,
			   FIELD_PREP(DEFAULT_FAST_LINK_TRAIN_EN, 0));

	dw_dp_hpd_init(dp);
	dw_dp_aux_init(dp);

	regmap_update_bits(dp->regmap, DPTX_GENERAL_INTERRUPT_ENABLE,
			   VIDEO_FIFO_OVERFLOW_STREAM0 | VIDEO_FIFO_OVERFLOW_STREAM1 |
			   VIDEO_FIFO_OVERFLOW_STREAM2 | VIDEO_FIFO_OVERFLOW_STREAM3,
			   FIELD_PREP(VIDEO_FIFO_OVERFLOW_STREAM0, 1) |
			   FIELD_PREP(VIDEO_FIFO_OVERFLOW_STREAM1, 1) |
			   FIELD_PREP(VIDEO_FIFO_OVERFLOW_STREAM2, 1) |
			   FIELD_PREP(VIDEO_FIFO_OVERFLOW_STREAM3, 1));
}

static void dw_dp_encoder_enable(struct drm_encoder *encoder)
{

}

static void dw_dp_encoder_atomic_disable(struct drm_encoder *encoder,
					 struct drm_atomic_state *state)
{
	struct dw_dp *dp = encoder_to_dp(encoder);
	struct drm_crtc *old_crtc, *new_crtc;
	struct rockchip_crtc_state *s;

	old_crtc = drm_atomic_get_old_crtc_for_encoder(state, encoder);
	new_crtc = drm_atomic_get_new_crtc_for_encoder(state, encoder);

	if (old_crtc && old_crtc != new_crtc) {
		s = to_rockchip_crtc_state(old_crtc->state);

		if (dp->split_mode)
			s->output_if &= ~(VOP_OUTPUT_IF_DP0 | VOP_OUTPUT_IF_DP1);
		else
			s->output_if &= ~(dp->id ? VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0);
		s->output_if_left_panel &= ~(dp->id ? VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0);
	}
}

static void dw_dp_mode_fixup(struct dw_dp *dp, struct drm_display_mode *adjusted_mode)
{
	int min_hbp = 16;
	int min_hsync = 9;
	int align_hfp = 2;
	int unalign_pixel;

	/*
	 * Here are some limits for the video timing output by dp port:
	 * 1. the hfp should be 2 pixels align;
	 * 2. the minimum hsync should be 9 pixel;
	 * 3. the minimum hbp should be 16 pixel;
	 */

	if (dp->split_mode || dp->dual_connector_split) {
		min_hbp *= 2;
		min_hsync *= 2;
		align_hfp *= 2;
	}

	unalign_pixel = (adjusted_mode->hsync_start - adjusted_mode->hdisplay) % align_hfp;
	if (unalign_pixel) {
		adjusted_mode->hsync_start += align_hfp - unalign_pixel;
		dev_warn(dp->dev, "hfp is not align, fixup to align hfp\n");
	}

	if (adjusted_mode->hsync_end - adjusted_mode->hsync_start < min_hsync) {
		adjusted_mode->hsync_end = adjusted_mode->hsync_start + min_hsync;
		dev_warn(dp->dev, "hsync is too narrow, fixup to min hsync:%d\n", min_hsync);
	}
	if (adjusted_mode->htotal - adjusted_mode->hsync_end < min_hbp) {
		adjusted_mode->htotal = adjusted_mode->hsync_end + min_hbp;
		dev_warn(dp->dev, "hbp is too narrow, fixup to min hbp:%d\n", min_hbp);
	}
}

static int dw_dp_get_eotf(struct drm_connector_state *conn_state)
{
	if (conn_state->hdr_output_metadata) {
		struct hdr_output_metadata *hdr_metadata =
			(struct hdr_output_metadata *)conn_state->hdr_output_metadata->data;

		return hdr_metadata->hdmi_metadata_type1.eotf;
	}

	return HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
}

static int dw_dp_encoder_atomic_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct dw_dp *dp = encoder_to_dp(encoder);
	struct dw_dp_video *video = &dp->video;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_display_info *di = &conn_state->connector->display_info;

	dp->eotf_type = dw_dp_get_eotf(conn_state);
	switch (video->color_format) {
	case DRM_COLOR_FORMAT_YCBCR420:
		s->output_mode = ROCKCHIP_OUT_MODE_YUV420;
		break;
	case DRM_COLOR_FORMAT_YCBCR422:
		s->output_mode = ROCKCHIP_OUT_MODE_YUV422;
		break;
	case DRM_COLOR_FORMAT_RGB444:
	case DRM_COLOR_FORMAT_YCBCR444:
	default:
		s->output_mode = ROCKCHIP_OUT_MODE_AAAA;
		break;
	}

	if (dp->split_mode) {
		s->output_flags |= ROCKCHIP_OUTPUT_DUAL_CHANNEL_LEFT_RIGHT_MODE;
		s->output_flags |= dp->id ? ROCKCHIP_OUTPUT_DATA_SWAP : 0;
		s->output_if |= VOP_OUTPUT_IF_DP0 | VOP_OUTPUT_IF_DP1;
		s->output_if_left_panel |= dp->id ? VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0;
	} else if (dp->dual_connector_split) {
		s->output_flags |= ROCKCHIP_OUTPUT_DUAL_CONNECTOR_SPLIT_MODE;
		s->output_if |= dp->id ? VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0;
		if (dp->left_display)
			s->output_if_left_panel |= dp->id ?
					VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0;
	} else {
		s->output_if |= dp->id ? VOP_OUTPUT_IF_DP1 : VOP_OUTPUT_IF_DP0;
	}

	s->output_type = DRM_MODE_CONNECTOR_DisplayPort;
	s->bus_format = video->bus_format;
	s->bus_flags = di->bus_flags;
	s->tv_state = &conn_state->tv;
	s->eotf = dp->eotf_type;

	if (video->color_format == DRM_COLOR_FORMAT_RGB444)
		s->color_range = DRM_COLOR_YCBCR_FULL_RANGE;
	else
		s->color_range = DRM_COLOR_YCBCR_LIMITED_RANGE;

	if (dw_dp_is_hdr_eotf(s->eotf))
		s->color_encoding = DRM_COLOR_YCBCR_BT2020;
	else
		s->color_encoding = DRM_COLOR_YCBCR_BT709;

	dw_dp_mode_fixup(dp, &crtc_state->adjusted_mode);

	return 0;
}

static enum drm_mode_status dw_dp_encoder_mode_valid(struct drm_encoder *encoder,
						     const struct drm_display_mode *mode)
{
	struct drm_crtc *crtc = encoder->crtc;
	struct drm_device *dev = encoder->dev;
	struct rockchip_crtc_state *s;

	if (crtc) {
		s = to_rockchip_crtc_state(crtc->state);
		s->output_type = DRM_MODE_CONNECTOR_DisplayPort;
	} else {
		drm_for_each_crtc(crtc, dev) {
			if (!drm_encoder_crtc_ok(encoder, crtc))
				continue;

			s = to_rockchip_crtc_state(crtc->state);
			s->output_type = DRM_MODE_CONNECTOR_DisplayPort;
		}
	}

	return MODE_OK;
}

static const struct drm_encoder_helper_funcs dw_dp_encoder_helper_funcs = {
	.enable			= dw_dp_encoder_enable,
	.atomic_disable		= dw_dp_encoder_atomic_disable,
	.atomic_check		= dw_dp_encoder_atomic_check,
	.mode_valid		= dw_dp_encoder_mode_valid,
};

static int dw_dp_aux_write_data(struct dw_dp *dp, const u8 *buffer, size_t size)
{
	size_t i, j;

	for (i = 0; i < DIV_ROUND_UP(size, 4); i++) {
		size_t num = min_t(size_t, size - i * 4, 4);
		u32 value = 0;

		for (j = 0; j < num; j++)
			value |= buffer[i * 4 + j] << (j * 8);

		regmap_write(dp->regmap, DPTX_AUX_DATA0 + i * 4, value);
	}

	return size;
}

static int dw_dp_aux_read_data(struct dw_dp *dp, u8 *buffer, size_t size)
{
	size_t i, j;

	for (i = 0; i < DIV_ROUND_UP(size, 4); i++) {
		size_t num = min_t(size_t, size - i * 4, 4);
		u32 value;

		regmap_read(dp->regmap, DPTX_AUX_DATA0 + i * 4, &value);

		for (j = 0; j < num; j++)
			buffer[i * 4 + j] = value >> (j * 8);
	}

	return size;
}

static ssize_t dw_dp_aux_transfer(struct drm_dp_aux *aux,
				  struct drm_dp_aux_msg *msg)
{
	struct dw_dp *dp = container_of(aux, struct dw_dp, aux);
	unsigned long timeout = msecs_to_jiffies(10);
	u32 status, value;
	ssize_t ret = 0;

	if (WARN_ON(msg->size > 16))
		return -E2BIG;

	pm_runtime_get_sync(dp->dev);
	phy_set_mode_ext(dp->phy, PHY_MODE_DP, 0);

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
		ret = dw_dp_aux_write_data(dp, msg->buffer, msg->size);
		if (ret < 0)
			goto out;
		break;
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ:
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	if (msg->size > 0)
		value = FIELD_PREP(AUX_LEN_REQ, msg->size - 1);
	else
		value = FIELD_PREP(I2C_ADDR_ONLY, 1);
	value |= FIELD_PREP(AUX_CMD_TYPE, msg->request);
	value |= FIELD_PREP(AUX_ADDR, msg->address);
	regmap_write(dp->regmap, DPTX_AUX_CMD, value);

	status = wait_for_completion_timeout(&dp->complete, timeout);
	if (!status) {
		dev_dbg(dp->dev, "timeout waiting for AUX reply\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	regmap_read(dp->regmap, DPTX_AUX_STATUS, &value);
	if (value & AUX_TIMEOUT) {
		ret = -ETIMEDOUT;
		goto out;
	}

	msg->reply = FIELD_GET(AUX_STATUS, value);

	if (msg->size > 0 && msg->reply == DP_AUX_NATIVE_REPLY_ACK) {
		if (msg->request & DP_AUX_I2C_READ) {
			size_t count = FIELD_GET(AUX_BYTES_READ, value) - 1;

			if (count != msg->size) {
				ret = -EBUSY;
				goto out;
			}

			ret = dw_dp_aux_read_data(dp, msg->buffer, count);
			if (ret < 0)
				goto out;
		}
	}

out:
	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);

	return ret;
}

static ssize_t dw_dp_sim_aux_transfer(struct drm_dp_aux *aux,
				      struct drm_dp_aux_msg *msg)
{
	struct dw_dp *dp = container_of(aux, struct dw_dp, aux);

	if (dp->aux_client && dp->aux_client->transfer)
		return dp->aux_client->transfer(dp->aux_client, aux, msg);
	else
		return dw_dp_aux_transfer(aux, msg);
}

static enum drm_mode_status
dw_dp_bridge_mode_valid(struct drm_bridge *bridge,
			const struct drm_display_info *info,
			const struct drm_display_mode *mode)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct dw_dp_link *link = &dp->link;
	struct drm_display_mode m = {};
	u32 min_bpp;

	drm_mode_copy(&m, mode);

	if (dp->split_mode || dp->dual_connector_split)
		drm_mode_convert_to_origin_mode(&m);

	if (info->color_formats & DRM_COLOR_FORMAT_YCBCR420 &&
	    link->vsc_sdp_extension_for_colorimetry_supported &&
	    (drm_mode_is_420_only(info, &m) || drm_mode_is_420_also(info, &m)))
		min_bpp = 12;
	else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
		min_bpp = 16;
	else if (info->color_formats & DRM_COLOR_FORMAT_RGB444)
		min_bpp = 18;
	else
		min_bpp = 24;

	if (!link->vsc_sdp_extension_for_colorimetry_supported &&
	    drm_mode_is_420_only(info, &m))
		return MODE_NO_420;

	if (!dw_dp_bandwidth_ok(dp, &m, min_bpp, link->lanes, link->max_rate))
		return MODE_CLOCK_HIGH;

	if (m.flags & DRM_MODE_FLAG_DBLCLK)
		return MODE_H_ILLEGAL;

	return MODE_OK;
}

static void _dw_dp_loader_protect(struct dw_dp *dp, bool on)
{
	struct dw_dp_link *link = &dp->link;
	struct drm_connector *conn = &dp->connector;
	struct drm_display_info *di = &conn->display_info;

	u32 value;

	if (on) {
		if (dp->dynamic_pd_ctrl)
			pm_runtime_get_sync(dp->dev);
		di->color_formats = DRM_COLOR_FORMAT_RGB444;
		di->bpc = 8;

		regmap_read(dp->regmap, DPTX_PHYIF_CTRL, &value);
		switch (FIELD_GET(PHY_LANES, value)) {
		case 2:
			link->lanes = 4;
			break;
		case 1:
			link->lanes = 2;
			break;
		case 0:
			fallthrough;
		default:
			link->lanes = 1;
			break;
		}

		switch (FIELD_GET(PHY_RATE, value)) {
		case 3:
			link->rate = 810000;
			link->max_rate = 810000;
			break;
		case 2:
			link->rate = 540000;
			link->max_rate = 540000;
			break;
		case 1:
			link->rate = 270000;
			link->max_rate = 270000;
			break;
		case 0:
			fallthrough;
		default:
			link->rate = 162000;
			link->max_rate = 162000;
			break;
		}

		extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, true);
		dw_dp_audio_handle_plugged_change(dp->audio, true);
		phy_power_on(dp->phy);
	} else {
		phy_power_off(dp->phy);
		extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, false);
		dw_dp_audio_handle_plugged_change(dp->audio, false);

		if (dp->dynamic_pd_ctrl) {
			pm_runtime_mark_last_busy(dp->dev);
			pm_runtime_put_autosuspend(dp->dev);
		}
	}
}

static int dw_dp_loader_protect(struct drm_encoder *encoder, bool on)
{
	struct dw_dp *dp = encoder_to_dp(encoder);

	dp->is_loader_protect = true;
	_dw_dp_loader_protect(dp, on);
	if (dp->right)
		_dw_dp_loader_protect(dp->right, on);

	return 0;
}

static void dw_dp_mst_connector_destroy(struct drm_connector *connector)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);

	list_del(&mst_conn->head);
	drm_connector_cleanup(&mst_conn->connector);

	kfree(mst_conn);
}

static int dw_dp_mst_connector_late_register(struct drm_connector *connector)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);
	int ret;

	ret = drm_dp_mst_connector_late_register(connector, mst_conn->port);

	return ret;
}

static void dw_dp_mst_connector_early_unregister(struct drm_connector *connector)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);

	drm_dp_mst_connector_early_unregister(connector, mst_conn->port);
}

static const struct drm_connector_funcs dw_dp_mst_connector_funcs = {
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.reset			= drm_atomic_helper_connector_reset,
	.destroy		= dw_dp_mst_connector_destroy,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
	.late_register		= dw_dp_mst_connector_late_register,
	.early_unregister	= dw_dp_mst_connector_early_unregister,
};

static struct drm_bridge *dw_dp_mst_connector_get_bridge(struct dw_dp_mst_conn *mst_conn)
{
	struct dw_dp *dp = mst_conn->dp;
	int i;

	if (!dp->is_fix_port)
		return NULL;

	for (i = 0; i < dp->mst_port_num; i++)
		if (dp->mst_enc[i].fix_port_num == mst_conn->port->port_num)
			return dp->mst_enc[i].next_bridge;

	return NULL;
}

static int dw_dp_mst_connector_get_modes(struct drm_connector *connector)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
					  struct dw_dp_mst_conn, connector);
	struct dw_dp *dp = mst_conn->dp;
	struct drm_bridge *bridge;
	struct edid *edid;
	int num_modes = 0;

	if (dp->is_fix_port) {
		bridge = dw_dp_mst_connector_get_bridge(mst_conn);
		if (bridge) {
			num_modes = drm_bridge_get_modes(bridge, connector);
			if (num_modes)
				return num_modes;
		}
	}

	edid = drm_dp_mst_get_edid(connector, &dp->mst_mgr, mst_conn->port);
	if (edid) {
		drm_connector_update_edid_property(connector, edid);
		num_modes = drm_add_edid_modes(connector, edid);
		kfree(edid);
	}

	return num_modes;
}

/*
 * For DP MST function, multi encoders will be register, when a connector attach to a crtc,
 * it must select a best encoder.
 * A simple method is to bind every DP MST encoder to a target crtc, for example:
 * bind DP MST encoder 0 to crtc 0
 * bind DP MST encoder 1 to crtc 1
 * bind DP MST encoder 2 to crtc 2
 * If different DP MST connector need attach to the same crtc, the above method may cause
 * conflict when assign encoder. In this case, assigned encoder ass follow:
 * 1. First loop, find the connector that will detach a crtc and not attach to a new
 *    crtc, remove this connector's encoder;
 * 2. Second loop, find the connector that first attach to a crtc. If a unused encoder is
 *    available, this connector will be assigned a encoder;
 * 3. This function will be called when each DP MST connector to find the best encoder.
 */

static void dw_dp_mst_assigned_encoder(struct dw_dp *dp, struct drm_atomic_state *state,
				       struct drm_crtc *crtc)
{
	struct dw_dp_mst_conn *mst_conn;
	struct drm_connector_state *new_con_state;
	struct drm_connector *connector;
	u32 encoder_mask = 0;
	int i;

	/* remove the detach connector */
	list_for_each_entry(mst_conn, &dp->mst_conn_list, head) {
		connector = &mst_conn->connector;
		new_con_state = drm_atomic_get_new_connector_state(state, connector);
		if (!new_con_state) {
			if (connector->state->best_encoder)
				encoder_mask |= drm_encoder_mask(connector->state->best_encoder);
			continue;
		}

		if (!new_con_state->crtc) {
			mst_conn->mst_enc = NULL;
			continue;
		}

		/* if the connector attach crtc is no change but it have
		 * a new connector state, mark it best_encoder.
		 */
		if (connector->state->crtc == new_con_state->crtc)
			encoder_mask |= drm_encoder_mask(connector->state->best_encoder);
	}

	dev_info(dp->dev, "assgned encoder_mask:%x\n", encoder_mask);

	/* assigned encoder for attached connector */
	list_for_each_entry(mst_conn, &dp->mst_conn_list, head) {
		u32 availble_encoders;

		connector = &mst_conn->connector;
		new_con_state = drm_atomic_get_new_connector_state(state, connector);
		if (!new_con_state)
			continue;

		if (!connector->state->crtc && new_con_state->crtc) {
			availble_encoders = encoder_mask ^ connector->possible_encoders;
			for (i = 0; i < dp->mst_port_num; i++) {
				if (drm_encoder_crtc_ok(&dp->mst_enc[i].encoder,
							new_con_state->crtc) &&
				    (availble_encoders &
				     drm_encoder_mask(&dp->mst_enc[i].encoder))) {
					mst_conn->mst_enc = &dp->mst_enc[i];
					encoder_mask |= drm_encoder_mask(&dp->mst_enc[i].encoder);
					break;
				}
			}
		}
	}
}

static struct drm_encoder*
dw_dp_mst_connector_atomic_best_encoder(struct drm_connector *connector,
					struct drm_atomic_state *state)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
					  struct dw_dp_mst_conn, connector);
	struct dw_dp *dp = mst_conn->dp;
	struct drm_connector_state *conn_state = drm_atomic_get_new_connector_state(state,
										    connector);

	if (!conn_state->crtc)
		return NULL;

	dw_dp_mst_assigned_encoder(dp, state, conn_state->crtc);

	return &mst_conn->mst_enc->encoder;
}

static int dw_dp_mst_connector_detect(struct drm_connector *connector,
			    struct drm_modeset_acquire_ctx *ctx, bool force)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
					  struct dw_dp_mst_conn, connector);
	struct dw_dp *dp = mst_conn->dp;
	u8 dpcd;
	int ret;

	if (drm_connector_is_unregistered(connector))
		return connector_status_disconnected;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_DPRX_FEATURE_ENUMERATION_LIST, &dpcd);
	if (ret < 0)
		return connector_status_disconnected;

	mst_conn->vsc_sdp_extension_for_colorimetry_supported =
		!!(dpcd & DP_VSC_SDP_EXT_FOR_COLORIMETRY_SUPPORTED);

	return drm_dp_mst_detect_port(connector, ctx, &dp->mst_mgr,
				      mst_conn->port);
}

static int dw_dp_mst_connector_atomic_check(struct drm_connector *connector,
				  struct drm_atomic_state *state)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);
	struct dw_dp *dp = mst_conn->dp;
	struct drm_connector_state *new_conn_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct drm_connector_state *old_conn_state =
		drm_atomic_get_old_connector_state(state, connector);
	struct drm_crtc *new_crtc = new_conn_state->crtc;
	struct drm_crtc_state *crtc_state;
	int ret;

	if (!old_conn_state->crtc)
		return 0;

	if (new_crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state, new_crtc);

		if (!crtc_state ||
		    !drm_atomic_crtc_needs_modeset(crtc_state) ||
		    crtc_state->enable)
			return 0;
	}

	ret =  drm_dp_atomic_release_time_slots(state, &dp->mst_mgr, mst_conn->port);

	return ret;
}

static enum drm_mode_status
dw_dp_mst_connector_mode_valid(struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);
	struct drm_dp_mst_port *port = mst_conn->port;
	struct dw_dp *dp = mst_conn->dp;
	u32 min_bpp = 24;

	if (drm_connector_is_unregistered(connector))
		return  MODE_ERROR;

	if (!dw_dp_bandwidth_ok(dp, mode, min_bpp, dp->link.lanes, dp->link.max_rate))
		return MODE_CLOCK_HIGH;

	if (drm_dp_calc_pbn_mode(mode->clock, min_bpp, false) > port->full_pbn)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static const struct drm_connector_helper_funcs dw_dp_mst_connector_helper_funcs = {
	.get_modes = dw_dp_mst_connector_get_modes,
	.atomic_best_encoder = dw_dp_mst_connector_atomic_best_encoder,
	.detect_ctx = dw_dp_mst_connector_detect,
	.atomic_check = dw_dp_mst_connector_atomic_check,
	.mode_valid = dw_dp_mst_connector_mode_valid,
};

static void dw_dp_mst_encoder_cleanup(struct drm_encoder *encoder)
{
	struct dw_dp_mst_enc *mst_enc = container_of(encoder, struct dw_dp_mst_enc, encoder);
	struct dw_dp *dp = mst_enc->dp;

	if (!dp->support_mst)
		return;

	drm_encoder_cleanup(encoder);

	if (dp->mst_mgr.dev)
		drm_dp_mst_topology_mgr_destroy(&dp->mst_mgr);
}

static const struct drm_encoder_funcs dw_dp_mst_encoder_funcs = {
	.destroy = dw_dp_mst_encoder_cleanup,
};

static void dw_dp_mst_enable(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_CCTL, ENABLE_MST_MODE,
			   FIELD_PREP(ENABLE_MST_MODE, 1));
}

__maybe_unused static void dw_dp_mst_disable(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_CCTL, ENABLE_MST_MODE,
			   FIELD_PREP(ENABLE_MST_MODE, 0));
}

static void dw_dp_clear_vcpid_table(struct dw_dp *dp)
{
	int i;

	for (i = 0; i < 8; i++)
		regmap_write(dp->regmap, DPTX_MST_VCP_TABLE_REG_N(i), 0);
}

static int dw_dp_initiate_mst_act(struct dw_dp *dp)
{
	int val, retries, ret = 0;

	regmap_update_bits(dp->regmap, DPTX_CCTL, DPTX_CCTL_INITIATE_MST_ACT,
			   FIELD_PREP(DPTX_CCTL_INITIATE_MST_ACT, 1));

	ret = regmap_read_poll_timeout(dp->regmap, DPTX_CCTL, val,
				       !FIELD_GET(DPTX_CCTL_INITIATE_MST_ACT, val),
				       200, 10000);
	/*
	 * if hardware not auto clear this bit until timeout, something may be
	 * wrong with the act, it should be cleared manually and retry again
	 */
	if (ret) {
		for (retries = 0; retries < 3; retries++) {
			regmap_update_bits(dp->regmap, DPTX_CCTL, DPTX_CCTL_INITIATE_MST_ACT,
				   FIELD_PREP(DPTX_CCTL_INITIATE_MST_ACT, 0));
			usleep_range(2000, 2010);
			dev_info(dp->dev, "act auto clear timeout, retry do it again\n");

			regmap_update_bits(dp->regmap, DPTX_CCTL, DPTX_CCTL_INITIATE_MST_ACT,
					   FIELD_PREP(DPTX_CCTL_INITIATE_MST_ACT, 1));

			ret = regmap_read_poll_timeout(dp->regmap, DPTX_CCTL, val,
						       !FIELD_GET(DPTX_CCTL_INITIATE_MST_ACT, val),
						       200, 10000);
			if (!ret)
				break;
		}
	}

	return ret;
}

static void dw_dp_set_vcpid_table_slot(struct dw_dp *dp, u32 slot, u32 stream_id)
{
	u32 offset;
	u32 mask;
	u32 val;

	offset = DPTX_MST_VCP_TABLE_REG_N(slot >> 3);
	mask = 0xf << ((slot % 8) * 4);
	val = stream_id << ((slot % 8) * 4);
	regmap_update_bits(dp->regmap, offset, mask, val);
}

static void dw_dp_set_vcpid_table_range(struct dw_dp *dp, u32 start, u32 count, u32 stream_id)
{
	int i;

	for (i = 0; i < count; i++)
		dw_dp_set_vcpid_table_slot(dp, start + i, stream_id + 1);
	dev_dbg(dp->dev, "set stream%d start:%d, conunt:%d\n", stream_id, start, count);
}

static int dw_dp_set_vcpid_tables(struct dw_dp *dp,
				  struct drm_dp_mst_topology_state *mst_state)
{
	struct dw_dp_mst_enc *mst_enc;
	struct dw_dp_mst_conn *mst_conn;
	struct drm_dp_mst_atomic_payload *payload;
	int i;

	dw_dp_clear_vcpid_table(dp);

	for (i = 0; i < dp->mst_port_num; i++) {
		mst_enc = &dp->mst_enc[i];
		mst_conn = mst_enc->mst_conn;
		if (!mst_enc->active)
			continue;

		payload = drm_atomic_get_mst_payload_state(mst_state, mst_conn->port);
		dw_dp_set_vcpid_table_range(dp, payload->vc_start_slot, payload->time_slots,
					    mst_enc->stream_id);
	}

	return 0;
}

static void dw_dp_enable_vop_gate(struct dw_dp *dp, struct drm_crtc *crtc,
				  int stream_id, bool enable)
{
	int output_if;

	switch (stream_id) {
	case 0:
		output_if = VOP_OUTPUT_IF_DP0;
		break;
	case 1:
		output_if = VOP_OUTPUT_IF_DP1;
		break;
	case 2:
		output_if = VOP_OUTPUT_IF_DP2;
		break;
	default:
		dev_err(dp->dev, "invalid stream id:%d\n", stream_id);
		return;
	}

	if (enable)
		rockchip_drm_crtc_output_post_enable(crtc, output_if);
	else
		rockchip_drm_crtc_output_pre_disable(crtc, output_if);
}

/*
 * When DPTX controller config 2 pixe mode and work in sst mode,
 * and a low pixel clock image transmit in high link rate(HBR3),
 * some monitor may display flicker. To avoid this issue appear, use
 * lower link rate(HBR2) when transmit a low pixel clock image.
 */
static void dw_dp_limit_max_link_rate(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	struct dw_dp_video *video = &dp->video;

	link->rate = link->max_rate;

	if (dp->is_mst)
		return;

	if (dp->pixel_mode != DPTX_MP_DUAL_PIXEL)
		return;

	if (video->mode.clock > 50000)
		return;

	if (link->max_rate > 540000)
		link->rate = 540000;
}

static void dw_dp_mst_encoder_atomic_enable(struct drm_encoder *encoder,
					    struct drm_atomic_state *state)
{
	struct dw_dp_mst_enc *mst_enc = container_of(encoder, struct dw_dp_mst_enc, encoder);
	struct dw_dp *dp = mst_enc->dp;
	struct drm_dp_mst_topology_state *mst_state =
		drm_atomic_get_new_mst_topology_state(state, &dp->mst_mgr);
	struct drm_dp_mst_atomic_payload *payload;
	struct dw_dp_mst_conn *mst_conn = NULL;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_crtc_state *crtc_state = encoder->crtc->state;
	struct drm_display_mode *m = &mst_enc->video.mode;
	int ret;
	bool first_mst_stream;

	pm_runtime_get_sync(dp->dev);

	drm_mode_copy(m, &crtc_state->adjusted_mode);

	first_mst_stream = dp->active_mst_links == 0;
	dp->active_mst_links++;
	mst_enc->active = true;

	drm_connector_list_iter_begin(encoder->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->state->best_encoder == encoder) {
			mst_conn = container_of(connector, struct dw_dp_mst_conn, connector);
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (WARN_ON(!mst_conn))
		return;
	mst_enc->mst_conn = mst_conn;

	if (first_mst_stream) {
		dw_dp_limit_max_link_rate(dp);

		ret = phy_power_on(dp->phy);
		if (ret)
			dev_err(dp->dev, "phy power on failed: %d\n", ret);

		ret = dw_dp_link_power_up(dp);
		if (ret < 0)
			dev_err(dp->dev, "link power up failed: %d\n", ret);

		ret = dw_dp_link_train(dp);
		if (ret < 0)
			dev_err(dp->dev, "link training failed: %d\n", ret);

		dw_dp_mst_enable(dp);
		dw_dp_clear_vcpid_table(dp);
	}

	ret = drm_dp_send_power_updown_phy(&dp->mst_mgr, mst_conn->port, true);
	if (ret < 0)
		dev_err(dp->dev, "send power updown phy failed:%d\n", ret);

	payload = drm_atomic_get_mst_payload_state(mst_state, mst_conn->port);
	ret = drm_dp_add_payload_part1(&dp->mst_mgr, mst_state, payload);
	if (ret < 0)
		dev_err(dp->dev, "failed to create MST payload:%d\n", ret);

	dw_dp_set_vcpid_table_range(dp, payload->vc_start_slot, payload->time_slots,
				    mst_enc->stream_id);

	ret = dw_dp_initiate_mst_act(dp);
	if (ret < 0)
		dev_warn(dp->dev, "failed to initial mst act:%d\n", ret);

	ret = drm_dp_check_act_status(&dp->mst_mgr);
	if (ret < 0)
		dev_err(dp->dev, "failed to check act status:%d\n", ret);

	ret = dw_dp_video_enable(dp, &mst_enc->video, mst_enc->stream_id);
	if (ret < 0)
		dev_err(dp->dev, "failed to enable video: %d\n", ret);

	dw_dp_enable_vop_gate(dp, encoder->crtc, mst_enc->stream_id, true);
	drm_dp_add_payload_part2(&dp->mst_mgr, state, payload);
}

static void dw_dp_link_disable(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;

	if (dw_dp_detect(dp))
		dw_dp_link_power_down(dp);

	dw_dp_phy_xmit_enable(dp, 0);

	phy_power_off(dp->phy);

	link->train.clock_recovered = false;
	link->train.channel_equalized = false;
}

static void dw_dp_mst_encoder_atomic_disable(struct drm_encoder *encoder,
					     struct drm_atomic_state *state)
{
	struct dw_dp_mst_enc *mst_enc = container_of(encoder, struct dw_dp_mst_enc, encoder);
	struct dw_dp_mst_conn *mst_conn = mst_enc->mst_conn;
	struct dw_dp *dp = mst_enc->dp;
	struct drm_dp_mst_topology_state *old_mst_state =
		drm_atomic_get_old_mst_topology_state(state, &dp->mst_mgr);
	struct drm_dp_mst_topology_state *new_mst_state =
		drm_atomic_get_new_mst_topology_state(state, &dp->mst_mgr);
	const struct drm_dp_mst_atomic_payload *old_payload =
		drm_atomic_get_mst_payload_state(old_mst_state, mst_conn->port);
	struct drm_dp_mst_atomic_payload *new_payload =
		drm_atomic_get_mst_payload_state(new_mst_state, mst_conn->port);
	struct drm_crtc *crtc = encoder->crtc;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	int ret;

	dp->active_mst_links--;
	mst_enc->active = false;

	dw_dp_enable_vop_gate(dp, encoder->crtc, mst_enc->stream_id, false);

	if (!(crtc->state->encoder_mask & drm_encoder_mask(encoder))) {
		switch (mst_enc->stream_id) {
		case 0:
			s->output_if &= ~VOP_OUTPUT_IF_DP0;
			break;
		case 1:
			s->output_if &= ~VOP_OUTPUT_IF_DP1;
			break;
		case 2:
			s->output_if &= ~VOP_OUTPUT_IF_DP2;
			break;
		default:
			dev_err(dp->dev, "invalid stream id:%d\n", mst_enc->stream_id);
		}
	}

	drm_dp_remove_payload(&dp->mst_mgr, new_mst_state, old_payload, new_payload);

	dw_dp_set_vcpid_tables(dp, new_mst_state);

	ret = dw_dp_initiate_mst_act(dp);
	if (ret < 0)
		dev_err(dp->dev, "failed to initial mst act:%d\n", ret);

	drm_dp_check_act_status(&dp->mst_mgr);

	drm_dp_send_power_updown_phy(&dp->mst_mgr, mst_conn->port, false);

	dw_dp_video_disable(dp, mst_enc->stream_id);
	if (!dp->active_mst_links)
		dw_dp_link_disable(dp);

	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);
}

static int dw_dp_mst_encoder_atomic_check(struct drm_encoder *encoder,
					  struct drm_crtc_state *crtc_state,
					  struct drm_connector_state *conn_state)
{
	struct dw_dp_mst_enc *mst_enc = container_of(encoder, struct dw_dp_mst_enc, encoder);
	struct dw_dp *dp = mst_enc->dp;
	struct dw_dp_video *video = &mst_enc->video;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_display_info *di = &conn_state->connector->display_info;
	const struct dw_dp_output_format *fmt =
		dw_dp_get_output_format(MEDIA_BUS_FMT_RGB888_1X24);
	struct drm_atomic_state *state = crtc_state->state;
	struct drm_connector *connector = conn_state->connector;
	struct dw_dp_mst_conn *mst_conn = container_of(connector,
						       struct dw_dp_mst_conn, connector);
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
	int pbn, slot;

	mst_state = drm_atomic_get_mst_topology_state(crtc_state->state, &dp->mst_mgr);
	if (IS_ERR(mst_state))
		return PTR_ERR(mst_state);

	video->video_mapping = fmt->video_mapping;
	video->color_format = fmt->color_format;
	video->bus_format = fmt->bus_format;
	video->bpc = fmt->bpc;
	video->bpp = fmt->bpp;

	switch (video->color_format) {
	case DRM_COLOR_FORMAT_YCBCR420:
		s->output_mode = ROCKCHIP_OUT_MODE_YUV420;
		break;
	case DRM_COLOR_FORMAT_YCBCR422:
		s->output_mode = ROCKCHIP_OUT_MODE_S888_DUMMY;
		break;
	case DRM_COLOR_FORMAT_RGB444:
	case DRM_COLOR_FORMAT_YCBCR444:
	default:
		s->output_mode = ROCKCHIP_OUT_MODE_AAAA;
		break;
	}

	switch (mst_enc->stream_id) {
	case 0:
		s->output_if |= VOP_OUTPUT_IF_DP0;
		break;
	case 1:
		s->output_if |= VOP_OUTPUT_IF_DP1;
		break;
	case 2:
		s->output_if |= VOP_OUTPUT_IF_DP2;
		break;
	default:
		dev_err(dp->dev, "invalid stream id:%d\n", mst_enc->stream_id);
		return -EINVAL;
	}

	s->output_type = DRM_MODE_CONNECTOR_DisplayPort;
	s->bus_format = video->bus_format;
	s->bus_flags = di->bus_flags;
	s->tv_state = &conn_state->tv;
	s->color_encoding = DRM_COLOR_YCBCR_BT709;

	if (!mst_state->pbn_div) {
		mst_state->pbn_div = drm_dp_get_vc_payload_bw(&dp->mst_mgr, dp->link.rate,
							      dp->link.lanes);
	}
	pbn = drm_dp_calc_pbn_mode(crtc_state->adjusted_mode.crtc_clock, video->bpp, false);
	slot = drm_dp_atomic_find_time_slots(state, &dp->mst_mgr, mst_conn->port, pbn);
	if (slot < 0) {
		dev_err(dp->dev, "invalid slot:%d\n", slot);
		return -EINVAL;
	}

	drm_dp_mst_update_slots(mst_state, DP_CAP_ANSI_8B10B);

	payload = drm_atomic_get_mst_payload_state(mst_state, mst_conn->port);
	if (dp->aux_client && !payload->vcpi) {
		payload->vcpi = mst_enc->stream_id + 1;
		dev_info(dp->dev, "[MST PORT:%p] assigned VCPI #%d\n",
			 payload->port, payload->vcpi);
		mst_state->payload_mask |= BIT(payload->vcpi - 1);
	}

	return 0;
}

static enum drm_mode_status dw_dp_mst_encoder_mode_valid(struct drm_encoder *encoder,
							 const struct drm_display_mode *mode)
{
	struct dw_dp_mst_enc *mst_enc = container_of(encoder, struct dw_dp_mst_enc, encoder);
	struct dw_dp *dp = mst_enc->dp;
	const struct dw_dp_chip_data *data = dp->chip_data;
	const struct dw_dp_port_caps *cap = &data->caps[mst_enc->stream_id];

	if (mode->hdisplay > cap->max_hactive)
		return MODE_BAD_HVALUE;

	if (mode->vdisplay > cap->max_vactive)
		return MODE_BAD_VVALUE;

	if (mode->clock > cap->max_pixel_clock)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static const struct drm_encoder_helper_funcs dw_dp_mst_encoder_help_funcs = {
	.atomic_enable		= dw_dp_mst_encoder_atomic_enable,
	.atomic_disable		= dw_dp_mst_encoder_atomic_disable,
	.atomic_check		= dw_dp_mst_encoder_atomic_check,
	.mode_valid		= dw_dp_mst_encoder_mode_valid,
};

static struct drm_connector *
dw_dp_add_mst_connector(struct drm_dp_mst_topology_mgr *mgr, struct drm_dp_mst_port *port,
			const char *pathprop)
{
	struct dw_dp *dp = container_of(mgr, struct dw_dp, mst_mgr);
	struct dw_dp_mst_conn *mst_conn;
	struct drm_mode_config *conf = &mgr->dev->mode_config;
	int ret, i;

	mst_conn = kzalloc(sizeof(*mst_conn), GFP_KERNEL);
	if (!mst_conn)
		return NULL;

	mst_conn->port = port;
	mst_conn->dp = dp;
	drm_dp_mst_get_port_malloc(port);
	ret = drm_connector_init(mgr->dev, &mst_conn->connector, &dw_dp_mst_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		drm_dp_mst_put_port_malloc(port);
		kfree(mst_conn);
		return NULL;
	}

	drm_connector_helper_add(&mst_conn->connector, &dw_dp_mst_connector_helper_funcs);
	mst_conn->connector.funcs->reset(&mst_conn->connector);
	for (i = 0; i < dp->mst_port_num; i++) {
		if (!of_device_is_available(dp->mst_enc[i].port_node))
			continue;
		if (dp->is_fix_port && dp->mst_enc[i].fix_port_num != port->port_num)
			continue;
		ret = drm_connector_attach_encoder(&mst_conn->connector, &dp->mst_enc[i].encoder);
		if (ret)
			goto err;
	}

	drm_object_attach_property(&mst_conn->connector.base, conf->tv_brightness_property, 50);
	drm_object_attach_property(&mst_conn->connector.base, conf->tv_contrast_property, 50);
	drm_object_attach_property(&mst_conn->connector.base, conf->tv_saturation_property, 50);
	drm_object_attach_property(&mst_conn->connector.base, conf->tv_hue_property, 50);
	mst_conn->connector.state->tv.brightness = 50;
	mst_conn->connector.state->tv.contrast = 50;
	mst_conn->connector.state->tv.saturation = 50;
	mst_conn->connector.state->tv.hue = 50;

	drm_object_attach_property(&mst_conn->connector.base, mgr->dev->mode_config.path_property,
				   0);
	ret = drm_connector_set_path_property(&mst_conn->connector, pathprop);
	if (ret)
		goto err;

	list_add_tail(&mst_conn->head, &dp->mst_conn_list);

	return &mst_conn->connector;
err:
	drm_connector_cleanup(&mst_conn->connector);
	kfree(mst_conn);
	return NULL;
}

static const struct drm_dp_mst_topology_cbs mst_cbs = {
	.add_connector = dw_dp_add_mst_connector,
};

static bool
dw_dp_create_fake_mst_encoders(struct dw_dp *dp)
{
	int i;

	for (i = 0; i < dp->mst_port_num; i++) {
		if (!of_device_is_available(dp->mst_enc[i].port_node))
			continue;
		drm_encoder_init(dp->encoder.dev, &dp->mst_enc[i].encoder,
				 &dw_dp_mst_encoder_funcs, DRM_MODE_ENCODER_DPMST,
				 "DP-MST %d", i);
		dp->mst_enc[i].encoder.possible_crtcs =
			rockchip_drm_of_find_possible_crtcs(dp->encoder.dev,
							    dp->mst_enc[i].port_node);
		drm_encoder_helper_add(&dp->mst_enc[i].encoder, &dw_dp_mst_encoder_help_funcs);
		dp->mst_enc[i].stream_id = i;
		dp->mst_enc[i].dp = dp;
	}

	return true;
}

static int dw_dp_mst_get_fix_port(struct dw_dp *dp)
{
	char *prop_name = "rockchip,mst-fixed-ports";
	int elem_len, ret, i;
	int elem_data[DPTX_MAX_STREAMS];

	if (!device_property_present(dp->dev, prop_name))
		return 0;

	elem_len = device_property_count_u32(dp->dev, prop_name);
	if (dp->mst_port_num != elem_len)
		return -EINVAL;

	ret = device_property_read_u32_array(dp->dev, prop_name, elem_data, elem_len);
	if (ret)
		return -EINVAL;

	dp->is_fix_port = true;

	for (i = 0; i < dp->mst_port_num; i++)
		dp->mst_enc[i].fix_port_num = elem_data[i];

	return 0;
}

static int dw_dp_mst_find_ext_bridges(struct dw_dp *dp)
{
	struct dw_dp_mst_enc *mst_enc;
	int i, ret;

	for (i = 0; i < dp->mst_port_num; i++) {
		mst_enc = &dp->mst_enc[i];
		if (!of_device_is_available(dp->mst_enc[i].port_node))
			continue;
		ret = drm_of_find_panel_or_bridge(mst_enc->port_node, 2, -1, NULL,
						  &mst_enc->next_bridge);
		if (ret < 0 && ret != -ENODEV)
			return ret;

		if (mst_enc->next_bridge) {
			ret = drm_bridge_attach(&mst_enc->encoder, mst_enc->next_bridge, NULL, 0);
			if (ret) {
				DRM_DEV_ERROR(dp->dev, "failed to attach next bridge: %d\n", ret);
				return ret;
			}
		}
	}

	return 0;
}

static int dw_dp_mst_encoder_init(struct dw_dp *dp, int conn_base_id)
{
	int ret;

	if (!dp->support_mst)
		return 0;

	INIT_LIST_HEAD(&dp->mst_conn_list);
	dp->mst_mgr.cbs = &mst_cbs;
	dw_dp_create_fake_mst_encoders(dp);
	ret = dw_dp_mst_get_fix_port(dp);
	if (ret)
		return ret;

	ret = dw_dp_mst_find_ext_bridges(dp);
	if (ret)
		return ret;
	ret = drm_dp_mst_topology_mgr_init(&dp->mst_mgr, dp->encoder.dev,
					   &dp->aux, 16, dp->mst_port_num, conn_base_id);
	if (ret)
		return ret;

	return 0;
}

static int dw_dp_connector_init(struct dw_dp *dp)
{
	struct drm_connector *connector = &dp->connector;
	struct drm_bridge *bridge = &dp->bridge;
	struct drm_property *prop;
	struct drm_device *dev = bridge->dev;
	struct rockchip_drm_private *private = dev->dev_private;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	if (dp->next_bridge && dp->next_bridge->ops & DRM_BRIDGE_OP_DETECT)
		connector->polled = DRM_CONNECTOR_POLL_CONNECT |
				    DRM_CONNECTOR_POLL_DISCONNECT;
	connector->ycbcr_420_allowed = true;
	connector->interlace_allowed = true;

	ret = drm_connector_init(bridge->dev, connector,
				 &dw_dp_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		DRM_DEV_ERROR(dp->dev, "Failed to initialize connector\n");
		return ret;
	}

	drm_connector_helper_add(connector,
				 &dw_dp_connector_helper_funcs);

	drm_connector_attach_encoder(connector, bridge->encoder);

	ret = dw_dp_mst_encoder_init(dp, connector->base.id);
	if (ret)
		return ret;
	prop = drm_property_create_enum(connector->dev, 0, RK_IF_PROP_COLOR_DEPTH,
					color_depth_enum_list,
					ARRAY_SIZE(color_depth_enum_list));
	if (!prop) {
		DRM_DEV_ERROR(dp->dev, "create color depth prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->color_depth_property = prop;
	drm_object_attach_property(&connector->base, prop, 0);

	prop = drm_property_create_enum(connector->dev, 0, RK_IF_PROP_COLOR_FORMAT,
					color_format_enum_list,
					ARRAY_SIZE(color_format_enum_list));
	if (!prop) {
		DRM_DEV_ERROR(dp->dev, "create color format prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->color_format_property = prop;
	drm_object_attach_property(&connector->base, prop, 0);

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_COLOR_DEPTH_CAPS,
					 0, 1 << RK_IF_DEPTH_MAX);
	if (!prop) {
		DRM_DEV_ERROR(dp->dev, "create color depth caps prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->color_depth_capacity = prop;
	drm_object_attach_property(&connector->base, prop, 0);

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_COLOR_FORMAT_CAPS,
					 0, 1 << RK_IF_FORMAT_MAX);
	if (!prop) {
		DRM_DEV_ERROR(dp->dev, "create color format caps prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->color_format_capacity = prop;
	drm_object_attach_property(&connector->base, prop, 0);

	ret = drm_connector_attach_content_protection_property(&dp->connector, true);
	if (ret) {
		dev_err(dp->dev, "failed to attach content protection: %d\n", ret);
		return ret;
	}

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_ENCRYPTED,
					 RK_IF_HDCP_ENCRYPTED_NONE, RK_IF_HDCP_ENCRYPTED_LEVEL2);
	if (!prop) {
		dev_err(dp->dev, "create hdcp encrypted prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->hdcp_state_property = prop;
	drm_object_attach_property(&connector->base, prop, RK_IF_HDCP_ENCRYPTED_NONE);

	prop = drm_property_create(connector->dev, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				   "HDR_PANEL_METADATA", 0);
	if (!prop) {
		DRM_DEV_ERROR(dp->dev, "create hdr metedata prop for dp%d failed\n", dp->id);
		return -ENOMEM;
	}
	dp->hdr_panel_metadata_property = prop;
	drm_object_attach_property(&connector->base, prop, 0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.hdr_output_metadata_property,
				   0);
	drm_object_attach_property(&dp->connector.base, private->connector_id_prop, dp->id);

	return 0;
}

static int dw_dp_bridge_attach(struct drm_bridge *bridge,
			       enum drm_bridge_attach_flags flags)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct drm_connector *connector;
	bool skip_connector = false;
	int ret;

	if (!bridge->encoder) {
		DRM_DEV_ERROR(dp->dev, "Parent encoder object not found");
		return -ENODEV;
	}

	ret = drm_of_find_panel_or_bridge(bridge->of_node, 1, -1, &dp->panel,
					  &dp->next_bridge);
	if (ret < 0 && ret != -ENODEV)
		return ret;

	if (dp->next_bridge) {
		struct drm_bridge *next_bridge = dp->next_bridge;

		ret = drm_bridge_attach(bridge->encoder, next_bridge, bridge,
					next_bridge->ops & DRM_BRIDGE_OP_MODES ?
					DRM_BRIDGE_ATTACH_NO_CONNECTOR : 0);
		if (ret) {
			DRM_DEV_ERROR(dp->dev, "failed to attach next bridge: %d\n", ret);
			return ret;
		}

		skip_connector = !(next_bridge->ops & DRM_BRIDGE_OP_MODES);
	}

	if (flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)
		return 0;

	if (!skip_connector) {
		ret = dw_dp_connector_init(dp);
		if (ret) {
			DRM_DEV_ERROR(dp->dev, "failed to create connector\n");
			return ret;
		}

		connector = &dp->connector;
	} else {
		struct list_head *connector_list =
			&bridge->dev->mode_config.connector_list;

		list_for_each_entry(connector, connector_list, head)
			if (drm_connector_has_possible_encoder(connector,
							       bridge->encoder))
				break;
	}

	dp->sub_dev.connector = connector;
	dp->sub_dev.of_node = dp->dev->of_node;
	dp->sub_dev.loader_protect = dw_dp_loader_protect;
	rockchip_drm_register_sub_dev(&dp->sub_dev);

	return 0;
}

static void dw_dp_bridge_detach(struct drm_bridge *bridge)
{
	struct dw_dp *dp = bridge_to_dp(bridge);

	drm_connector_cleanup(&dp->connector);
}

static void dw_dp_bridge_atomic_pre_enable(struct drm_bridge *bridge,
					   struct drm_bridge_state *bridge_state)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct dw_dp_video *video = &dp->video;
	struct drm_crtc_state *crtc_state = bridge->encoder->crtc->state;
	struct drm_display_mode *m = &video->mode;

	pm_runtime_get_sync(dp->dev);

	drm_mode_copy(m, &crtc_state->adjusted_mode);

	if (dp->split_mode || dp->dual_connector_split)
		drm_mode_convert_to_origin_mode(m);

	if (dp->panel)
		drm_panel_prepare(dp->panel);
}

static void
dw_dp_bridge_atomic_post_disable(struct drm_bridge *bridge,
				 struct drm_bridge_state *bridge_state)
{
	struct dw_dp *dp = bridge_to_dp(bridge);

	if (dp->panel)
		drm_panel_unprepare(dp->panel);

	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);
}

static bool dw_dp_needs_link_retrain(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 link_status[DP_LINK_STATUS_SIZE];

	if (!dw_dp_link_train_valid(&link->train))
		return false;

	if (drm_dp_dpcd_read_link_status(&dp->aux, link_status) < 0)
		return false;

	/* Retrain if Channel EQ or CR not ok */
	return !drm_dp_channel_eq_ok(link_status, dp->link.lanes);
}

static int dw_dp_link_enable(struct dw_dp *dp)
{
	int ret;

	dw_dp_limit_max_link_rate(dp);

	ret = phy_power_on(dp->phy);
	if (ret)
		return ret;

	ret = dw_dp_link_power_up(dp);
	if (ret < 0)
		return ret;

	ret = dw_dp_link_train(dp);
	if (ret < 0) {
		dev_err(dp->dev, "link training failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void dw_dp_bridge_atomic_enable(struct drm_bridge *bridge,
				       struct drm_bridge_state *old_state)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct drm_atomic_state *state = old_state->base.state;
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	int ret;

	connector = drm_atomic_get_new_connector_for_encoder(state, bridge->encoder);
	if (!connector) {
		dev_err(dp->dev, "failed to get connector\n");
		return;
	}

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state) {
		dev_err(dp->dev, "failed to get connector state\n");
		return;
	}

	set_bit(0, dp->sdp_reg_bank);

	ret = dw_dp_link_enable(dp);
	if (ret < 0) {
		dev_err(dp->dev, "failed to enable link: %d\n", ret);
		return;
	}

	ret = dw_dp_video_enable(dp, &dp->video, 0);
	if (ret < 0) {
		dev_err(dp->dev, "failed to enable video: %d\n", ret);
		return;
	}

	dw_dp_enable_vop_gate(dp, bridge->encoder->crtc, dp->id, true);
	if (conn_state->content_protection == DRM_MODE_CONTENT_PROTECTION_DESIRED)
		dw_dp_hdcp_enable(dp, conn_state->hdcp_content_type);

	if (dp->panel)
		drm_panel_enable(dp->panel);

	extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, true);
	dw_dp_audio_handle_plugged_change(dp->audio, true);
}

static void dw_dp_bridge_atomic_disable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_bridge_state)
{
	struct dw_dp *dp = bridge_to_dp(bridge);

	if (dp->panel)
		drm_panel_disable(dp->panel);

	dw_dp_enable_vop_gate(dp, bridge->encoder->crtc, dp->id, false);
	dw_dp_hdcp_disable(dp);
	dw_dp_video_disable(dp, 0);
	dw_dp_link_disable(dp);
	bitmap_zero(dp->sdp_reg_bank, SDP_REG_BANK_SIZE);

	extcon_set_state_sync(dp->audio->extcon, EXTCON_DISP_DP, false);
	dw_dp_audio_handle_plugged_change(dp->audio, false);
}

static bool dw_dp_detect_dpcd(struct dw_dp *dp)
{
	u8 value;
	int ret;

	ret = phy_power_on(dp->phy);
	if (ret)
		goto fail_power_on;

	ret = drm_dp_dpcd_readb(&dp->aux, DP_DPCD_REV, &value);
	if (ret < 0) {
		dev_err(dp->dev, "aux failed to read dpcd: %d\n", ret);
		goto fail_probe;
	}

	ret = dw_dp_link_probe(dp);
	if (ret) {
		dev_err(dp->dev, "failed to probe DP link: %d\n", ret);
		goto fail_probe;
	}

	phy_power_off(dp->phy);

	return true;

fail_probe:
	phy_power_off(dp->phy);
fail_power_on:
	return false;
}

static enum drm_connector_status dw_dp_bridge_detect(struct drm_bridge *bridge)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	enum drm_connector_status status = connector_status_connected;

	if (dp->panel)
		drm_panel_prepare(dp->panel);

	if (!dw_dp_detect(dp)) {
		status = connector_status_disconnected;
		goto out;
	}

	if (!dw_dp_detect_dpcd(dp)) {
		status = connector_status_disconnected;
		goto out;
	}

	if (dp->next_bridge) {
		struct drm_bridge *next_bridge = dp->next_bridge;

		if (next_bridge->ops & DRM_BRIDGE_OP_DETECT)
			status = drm_bridge_detect(next_bridge);
	}

out:
	if (dp->is_loader_protect) {
		dp->is_loader_protect = false;
		return status;
	}
	if (status == connector_status_disconnected) {
		if (dp->is_mst) {
			dev_info(dp->dev, "MST device may have disappeared\n");
			dp->is_mst = false;
			drm_dp_mst_topology_mgr_set_mst(&dp->mst_mgr, dp->is_mst);
			phy_power_off(dp->phy);
		}
	} else {
		if (dp->link.sink_support_mst && dp->mst_mgr.cbs) {
			dev_info(dp->dev, "MST device appeared\n");
			if (!dp->is_mst)
				phy_power_on(dp->phy);
			dp->is_mst = true;
			drm_dp_mst_topology_mgr_set_mst(&dp->mst_mgr, dp->is_mst);
			status = connector_status_disconnected;
		}
	}

	return status;
}

static struct edid *dw_dp_bridge_get_edid(struct drm_bridge *bridge,
					  struct drm_connector *connector)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct edid *edid;
	int ret;

	ret = phy_power_on(dp->phy);
	if (ret)
		return NULL;

	edid = drm_get_edid(connector, &dp->aux.ddc);

	phy_power_off(dp->phy);

	return edid;
}

static void dw_dp_swap_fmts(u32 *fmt, int count)
{
	int i;
	u32 temp_fmt;

	if (!count)
		return;

	for (i = 0; i < count / 2; i++) {
		temp_fmt = fmt[i];
		fmt[i] = fmt[count - i - 1];
		fmt[count - i - 1] = temp_fmt;
	}
}

static u32 *dw_dp_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
					struct drm_bridge_state *bridge_state,
					struct drm_crtc_state *crtc_state,
					struct drm_connector_state *conn_state,
					unsigned int *num_output_fmts)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct dw_dp_state *dp_state = connector_to_dp_state(conn_state);
	struct dw_dp_link *link = &dp->link;
	struct drm_display_info *di = &conn_state->connector->display_info;
	struct drm_display_mode mode = crtc_state->mode;
	u32 *output_fmts;
	unsigned int i, j = 0;

	dp->eotf_type = dw_dp_get_eotf(conn_state);

	if (dp->split_mode || dp->dual_connector_split)
		drm_mode_convert_to_origin_mode(&mode);

	if (dp->panel) {
		*num_output_fmts = 1;

		output_fmts = kzalloc(sizeof(*output_fmts), GFP_KERNEL);
		if (!output_fmts)
			return NULL;

		if (di->num_bus_formats && di->bus_formats)
			output_fmts[0] = di->bus_formats[0];
		else
			output_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;

		return output_fmts;
	}

	*num_output_fmts = 0;

	output_fmts = kcalloc(ARRAY_SIZE(possible_output_fmts),
			      sizeof(*output_fmts), GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(possible_output_fmts); i++) {
		const struct dw_dp_output_format *fmt = &possible_output_fmts[i];

		if (fmt->bpc > conn_state->max_bpc)
			continue;

		if (!(di->color_formats & fmt->color_format))
			continue;

		if (fmt->color_format == DRM_COLOR_FORMAT_YCBCR420 &&
		    !link->vsc_sdp_extension_for_colorimetry_supported)
			continue;

		if (!drm_mode_is_420(di, &mode) &&
		    fmt->color_format == DRM_COLOR_FORMAT_YCBCR420)
			continue;

		if (drm_mode_is_420_only(di, &mode) &&
		    fmt->color_format != DRM_COLOR_FORMAT_YCBCR420)
			continue;

		if (!dw_dp_bandwidth_ok(dp, &mode, fmt->bpp, link->lanes, link->max_rate))
			continue;

		if (dp_state->bpc != 0) {
			if (fmt->bpc != dp_state->bpc)
				continue;

			if (dp_state->color_format != RK_IF_FORMAT_YCBCR_HQ &&
			    dp_state->color_format != RK_IF_FORMAT_YCBCR_LQ &&
			    (fmt->color_format != BIT(dp_state->color_format)))
				continue;
		}

		if (dw_dp_is_hdr_eotf(dp->eotf_type) && fmt->bpc < 10)
			continue;

		output_fmts[j++] = fmt->bus_format;
	}

	if (dp_state->color_format == RK_IF_FORMAT_YCBCR_LQ)
		dw_dp_swap_fmts(output_fmts, j);

	*num_output_fmts = j;

	if (*num_output_fmts == 0) {
		dev_warn(dp->dev, "here is not satisfied the require bus format\n");
		dev_info(dp->dev,
			 "max bpc:%d, max fmt:%x, lanes:%d, rate:%d, bpc:%d, fmt:%d, eotf:%d\n",
			 conn_state->max_bpc, di->color_formats, link->lanes, link->max_rate,
			 dp_state->bpc, dp_state->color_format, dp->eotf_type);
	}

	return output_fmts;
}

static int dw_dp_bridge_atomic_check(struct drm_bridge *bridge,
				     struct drm_bridge_state *bridge_state,
				     struct drm_crtc_state *crtc_state,
				     struct drm_connector_state *conn_state)
{
	struct dw_dp *dp = bridge_to_dp(bridge);
	struct dw_dp_video *video = &dp->video;
	const struct dw_dp_output_format *fmt =
		dw_dp_get_output_format(bridge_state->output_bus_cfg.format);

	dev_dbg(dp->dev, "input format 0x%04x, output format 0x%04x\n",
		bridge_state->input_bus_cfg.format,
		bridge_state->output_bus_cfg.format);

	video->video_mapping = fmt->video_mapping;
	video->color_format = fmt->color_format;
	video->bus_format = fmt->bus_format;
	video->bpc = fmt->bpc;
	video->bpp = fmt->bpp;

	return 0;
}

static const struct drm_bridge_funcs dw_dp_bridge_funcs = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_get_input_bus_fmts = drm_atomic_helper_bridge_propagate_bus_fmt,
	.atomic_get_output_bus_fmts = dw_dp_bridge_atomic_get_output_bus_fmts,
	.attach = dw_dp_bridge_attach,
	.detach = dw_dp_bridge_detach,
	.mode_valid = dw_dp_bridge_mode_valid,
	.atomic_check = dw_dp_bridge_atomic_check,
	.atomic_pre_enable = dw_dp_bridge_atomic_pre_enable,
	.atomic_post_disable = dw_dp_bridge_atomic_post_disable,
	.atomic_enable = dw_dp_bridge_atomic_enable,
	.atomic_disable = dw_dp_bridge_atomic_disable,
	.detect = dw_dp_bridge_detect,
	.get_edid = dw_dp_bridge_get_edid,
};

static int dw_dp_link_retrain(struct dw_dp *dp)
{
	struct drm_device *dev = dp->bridge.dev;
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	if (!dw_dp_needs_link_retrain(dp))
		return 0;

	dev_dbg(dp->dev, "Retraining link\n");

	drm_modeset_acquire_init(&ctx, 0);
	for (;;) {
		ret = drm_modeset_lock(&dev->mode_config.connection_mutex, &ctx);
		if (ret != -EDEADLK)
			break;

		drm_modeset_backoff(&ctx);
	}

	ret = dw_dp_link_train(dp);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;
}

static u8 dw_dp_autotest_phy_pattern(struct dw_dp *dp)
{
	struct drm_dp_phy_test_params *data = &dp->compliance.test_data.phytest;

	if (drm_dp_get_phy_test_pattern(&dp->aux, data)) {
		dev_err(dp->dev, "DP Phy Test pattern AUX read failure\n");
		return DP_TEST_NAK;
	}

	/* Set test active flag here so userspace doesn't interrupt things */
	dp->compliance.test_active = true;

	return DP_TEST_ACK;
}

static void dw_dp_handle_test_request(struct dw_dp *dp)
{
	u8 response = DP_TEST_NAK;
	u8 request = 0;
	int status;

	status = drm_dp_dpcd_readb(&dp->aux, DP_TEST_REQUEST, &request);
	if (status <= 0) {
		dev_err(dp->dev, "Could not read test request from sink\n");
		goto update_status;
	}

	switch (request) {
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		dev_info(dp->dev, "PHY_PATTERN test requested\n");
		response = dw_dp_autotest_phy_pattern(dp);
		break;
	default:
		dev_warn(dp->dev, "Invalid test request '%02x'\n", request);
		break;
	}

	if (response & DP_TEST_ACK)
		dp->compliance.test_type = request;

update_status:
	status = drm_dp_dpcd_writeb(&dp->aux, DP_TEST_RESPONSE, response);
	if (status <= 0)
		dev_warn(dp->dev, "Could not write test response to sink\n");
}

static void dw_dp_hdcp_handle_cp_irq(struct dw_dp *dp)
{
	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, CP_IRQ, CP_IRQ);
	udelay(20);
	regmap_update_bits(dp->regmap, DPTX_HDCPCFG, CP_IRQ, 0);
}

static void dw_dp_check_service_irq(struct dw_dp *dp)
{
	struct dw_dp_link *link = &dp->link;
	u8 val;

	if (link->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (drm_dp_dpcd_readb(&dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, &val) != 1 || !val)
		return;

	drm_dp_dpcd_writeb(&dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, val);

	if (val & DP_AUTOMATED_TEST_REQUEST)
		dw_dp_handle_test_request(dp);

	if (val & DP_CP_IRQ)
		dw_dp_hdcp_handle_cp_irq(dp);

	if (val & DP_SINK_SPECIFIC_IRQ)
		dev_info(dp->dev, "Sink specific irq unhandled\n");
}

static void dw_dp_phy_pattern_update(struct dw_dp *dp)
{
	struct drm_dp_phy_test_params *data = &dp->compliance.test_data.phytest;

	switch (data->phy_pattern) {
	case DP_PHY_TEST_PATTERN_NONE:
		dev_info(dp->dev, "Disable Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 1));
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_NONE);
		break;
	case DP_PHY_TEST_PATTERN_D10_2:
		dev_info(dp->dev, "Set D10.2 Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 1));
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_1);
		break;
	case DP_PHY_TEST_PATTERN_ERROR_COUNT:
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 0));
		dev_info(dp->dev, "Set Error Count Phy Test Pattern\n");
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_SERM);
		break;
	case DP_PHY_TEST_PATTERN_PRBS7:
		dev_info(dp->dev, "Set PRBS7 Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 1));
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_PBRS7);
		break;
	case DP_PHY_TEST_PATTERN_80BIT_CUSTOM:
		dev_info(dp->dev, "Set 80Bit Custom Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 1));
		regmap_write(dp->regmap, DPTX_CUSTOMPAT0, 0x3e0f83e0);
		regmap_write(dp->regmap, DPTX_CUSTOMPAT1, 0x3e0f83e0);
		regmap_write(dp->regmap, DPTX_CUSTOMPAT2, 0x000f83e0);
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_CUSTOM_80BIT);
		break;
	case DP_PHY_TEST_PATTERN_CP2520:
		dev_info(dp->dev, "Set HBR2 compliance Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 0));
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_CP2520_1);
		break;
	case DP_PHY_TEST_PATTERN_SEL_MASK:
		dev_info(dp->dev, "Set TPS4  Phy Test Pattern\n");
		regmap_update_bits(dp->regmap, DPTX_CCTL, SCRAMBLE_DIS,
				   FIELD_PREP(SCRAMBLE_DIS, 0));
		dw_dp_phy_set_pattern(dp, DPTX_PHY_PATTERN_TPS_4);
		break;
	default:
		WARN(1, "Invalid Phy Test Pattern\n");
	}
}

static void dw_dp_process_phy_request(struct dw_dp *dp)
{
	struct drm_dp_phy_test_params *data = &dp->compliance.test_data.phytest;
	u8 link_status[DP_LINK_STATUS_SIZE], spread;
	int ret;

	ret = drm_dp_dpcd_read(&dp->aux, DP_LANE0_1_STATUS, link_status, DP_LINK_STATUS_SIZE);
	if (ret < 0) {
		dev_err(dp->dev, "failed to get link status\n");
		return;
	}

	ret = drm_dp_dpcd_readb(&dp->aux, DP_MAX_DOWNSPREAD, &spread);
	if (ret < 0) {
		dev_err(dp->dev, "failed to get spread\n");
		return;
	}

	dw_dp_phy_configure(dp, data->link_rate, data->num_lanes,
			    !!(spread & DP_MAX_DOWNSPREAD_0_5));
	dw_dp_link_get_adjustments(&dp->link, link_status);
	dw_dp_phy_update_vs_emph(dp, data->link_rate, data->num_lanes, &dp->link.train.adjust);
	dw_dp_phy_pattern_update(dp);
	drm_dp_set_phy_test_pattern(&dp->aux, data, link_status[DP_DPCD_REV]);

	dev_info(dp->dev, "phy test rate:%d, lane count:%d, ssc:%d, vs:%d, pe: %d\n",
		 data->link_rate, data->num_lanes, spread, dp->link.train.adjust.voltage_swing[0],
		 dp->link.train.adjust.pre_emphasis[0]);
}

static void dw_dp_phy_test(struct dw_dp *dp)
{
	struct drm_device *dev = dp->bridge.dev;
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	drm_modeset_acquire_init(&ctx, 0);

	for (;;) {
		ret = drm_modeset_lock(&dev->mode_config.connection_mutex, &ctx);
		if (ret != -EDEADLK)
			break;

		drm_modeset_backoff(&ctx);
	}

	dw_dp_process_phy_request(dp);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static bool dw_dp_hpd_short_pulse(struct dw_dp *dp)
{
	memset(&dp->compliance, 0, sizeof(dp->compliance));

	dw_dp_check_service_irq(dp);

	if (dw_dp_needs_link_retrain(dp))
		return false;

	switch (dp->compliance.test_type) {
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		return false;
	default:
		dev_warn(dp->dev, "test_type%lu is not support\n", dp->compliance.test_type);
		break;
	}

	return true;
}

static bool
dw_dp_get_sink_irq_esi(struct dw_dp *dp, u8 *esi)
{
	return drm_dp_dpcd_read(&dp->aux, DP_SINK_COUNT_ESI, esi, 4) == 4;
}

static bool dw_dp_ack_sink_irq_esi(struct dw_dp *dp, u8 esi[4])
{
	int retry;

	for (retry = 0; retry < 3; retry++) {
		if (drm_dp_dpcd_write(&dp->aux, DP_SINK_COUNT_ESI + 1,
				      &esi[1], 3) == 3)
			return true;
	}

	return false;
}
static bool dw_dp_check_mst_status(struct dw_dp *dp)
{
	bool link_ok = true;
	bool handled = false;

	for (;;) {
		u8 esi[4] = {};
		u8 ack[4] = {};

		if (!dw_dp_get_sink_irq_esi(dp, esi)) {
			dev_err(dp->dev, "failed to get ESI - device may have failed\n");
			link_ok = false;
			break;
		}

		drm_dp_mst_hpd_irq_handle_event(&dp->mst_mgr, esi, ack, &handled);

		if (!memchr_inv(ack, 0, sizeof(ack)))
			break;

		if (!dw_dp_ack_sink_irq_esi(dp, ack))
			dev_err(dp->dev, "failed to  ack ESI\n");

		if (ack[1] & (DP_DOWN_REP_MSG_RDY | DP_UP_REQ_MSG_RDY))
			drm_dp_mst_hpd_irq_send_new_request(&dp->mst_mgr);
	}

	return link_ok;
}

static void dw_dp_hpd_work(struct work_struct *work)
{
	struct dw_dp *dp = container_of(work, struct dw_dp, hpd_work);
	bool long_hpd;
	int ret;

	mutex_lock(&dp->irq_lock);
	long_hpd = dp->hotplug.long_hpd;
	mutex_unlock(&dp->irq_lock);

	dev_dbg(dp->dev, "got hpd irq - %s\n", long_hpd ? "long" : "short");

	if (!long_hpd) {
		phy_power_on(dp->phy);
		if (dp->is_mst) {
			dw_dp_check_mst_status(dp);
			phy_power_off(dp->phy);
			return;
		}

		if (dw_dp_hpd_short_pulse(dp)) {
			phy_power_off(dp->phy);
			return;
		}

		if (dp->compliance.test_active &&
		    dp->compliance.test_type == DP_TEST_LINK_PHY_TEST_PATTERN) {
			dw_dp_phy_test(dp);
			/* just do the PHY test and nothing else */
			phy_power_off(dp->phy);
			return;
		}

		ret = dw_dp_link_retrain(dp);
		if (ret)
			dev_warn(dp->dev, "Retrain link failed\n");
		phy_power_off(dp->phy);
	} else {
		drm_helper_hpd_irq_event(dp->bridge.dev);
	}
}

static void dw_dp_handle_hpd_event(struct dw_dp *dp)
{
	u32 value;

	mutex_lock(&dp->irq_lock);

	regmap_read(dp->regmap, DPTX_HPD_STATUS, &value);

	if (value & HPD_IRQ) {
		dev_dbg(dp->dev, "IRQ from the HPD\n");
		dp->hotplug.long_hpd = false;
		regmap_write(dp->regmap, DPTX_HPD_STATUS, HPD_IRQ);
	}

	if (value & HPD_HOT_PLUG) {
		dev_dbg(dp->dev, "Hot plug detected\n");
		dp->hotplug.long_hpd = true;
		regmap_write(dp->regmap, DPTX_HPD_STATUS, HPD_HOT_PLUG);
	}

	if (value & HPD_HOT_UNPLUG) {
		dev_dbg(dp->dev, "Unplug detected\n");
		dp->hotplug.long_hpd = true;
		regmap_write(dp->regmap, DPTX_HPD_STATUS, HPD_HOT_UNPLUG);
	}

	mutex_unlock(&dp->irq_lock);

	schedule_work(&dp->hpd_work);
}

static irqreturn_t dw_dp_irq_handler(int irq, void *data)
{
	struct dw_dp *dp = data;
	u32 value;

	regmap_read(dp->regmap, DPTX_GENERAL_INTERRUPT, &value);
	if (!value)
		return IRQ_NONE;

	if (value & HPD_EVENT)
		dw_dp_handle_hpd_event(dp);

	if (value & AUX_REPLY_EVENT) {
		regmap_write(dp->regmap, DPTX_GENERAL_INTERRUPT,
			     AUX_REPLY_EVENT);
		complete(&dp->complete);
	}

	if (value & HDCP_EVENT)
		dw_dp_handle_hdcp_event(dp);

	if (value & VIDEO_FIFO_OVERFLOW_STREAM0) {
		dev_err_ratelimited(dp->dev, "video fifo overflow stream0\n");
		regmap_write(dp->regmap, DPTX_GENERAL_INTERRUPT,
			     VIDEO_FIFO_OVERFLOW_STREAM0);
	}

	if (value & VIDEO_FIFO_OVERFLOW_STREAM1) {
		dev_err_ratelimited(dp->dev, "video fifo overflow stream1\n");
		regmap_write(dp->regmap, DPTX_GENERAL_INTERRUPT,
			     VIDEO_FIFO_OVERFLOW_STREAM1);
	}

	if (value & VIDEO_FIFO_OVERFLOW_STREAM2) {
		dev_err_ratelimited(dp->dev, "video fifo overflow stream2\n");
		regmap_write(dp->regmap, DPTX_GENERAL_INTERRUPT,
			     VIDEO_FIFO_OVERFLOW_STREAM2);
	}

	if (value & VIDEO_FIFO_OVERFLOW_STREAM3) {
		dev_err_ratelimited(dp->dev, "video fifo overflow stream3\n");
		regmap_write(dp->regmap, DPTX_GENERAL_INTERRUPT,
			     VIDEO_FIFO_OVERFLOW_STREAM3);
	}

	return IRQ_HANDLED;
}

static int dw_dp_audio_hw_params(struct device *dev, void *data,
				 struct hdmi_codec_daifmt *daifmt,
				 struct hdmi_codec_params *params)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct dw_dp_audio *audio = (struct dw_dp_audio *)data;

	u8 audio_data_in_en, num_channels, audio_inf_select;

	audio->channels = params->cea.channels;

	switch (params->cea.channels) {
	case 1:
		audio_data_in_en = 0x1;
		num_channels = 0x0;
		break;
	case 2:
		audio_data_in_en = 0x1;
		num_channels = 0x1;
		break;
	case 8:
		audio_data_in_en = 0xf;
		num_channels = 0x7;
		break;
	default:
		dev_err(dp->dev, "invalid channels %d\n", params->cea.channels);
		return -EINVAL;
	}

	switch (daifmt->fmt) {
	case HDMI_SPDIF:
		audio_inf_select = 0x1;
		audio->format = AFMT_SPDIF;
		break;
	case HDMI_I2S:
		audio_inf_select = 0x0;
		audio->format = AFMT_I2S;
		break;
	default:
		dev_err(dp->dev, "invalid daifmt %d\n", daifmt->fmt);
		return -EINVAL;
	}

	clk_prepare_enable(audio->spdif_clk);
	clk_prepare_enable(audio->i2s_clk);

	pm_runtime_get_sync(dp->dev);
	regmap_update_bits(dp->regmap, DPTX_AUD_CONFIG1_N(audio->id),
			   AUDIO_DATA_IN_EN | NUM_CHANNELS | AUDIO_DATA_WIDTH |
			   AUDIO_INF_SELECT,
			   FIELD_PREP(AUDIO_DATA_IN_EN, audio_data_in_en) |
			   FIELD_PREP(NUM_CHANNELS, num_channels) |
			   FIELD_PREP(AUDIO_DATA_WIDTH, params->sample_width) |
			   FIELD_PREP(AUDIO_INF_SELECT, audio_inf_select));
	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);

	/* Wait for inf switch */
	usleep_range(20, 40);
	if (audio->format == AFMT_I2S)
		clk_disable_unprepare(audio->spdif_clk);
	else if (audio->format == AFMT_SPDIF)
		clk_disable_unprepare(audio->i2s_clk);

	return 0;
}

static int dw_dp_audio_infoframe_send(struct dw_dp *dp, struct dw_dp_audio *audio)
{
	struct hdmi_audio_infoframe frame;
	struct dp_sdp_header header;
	u8 buffer[HDMI_INFOFRAME_HEADER_SIZE + HDMI_DRM_INFOFRAME_SIZE];
	u8 size = sizeof(buffer);
	int i, j, ret;

	header.HB0 = 0;
	header.HB1 = HDMI_INFOFRAME_TYPE_AUDIO;
	header.HB2 = 0x1b;
	header.HB3 = 0x48;

	ret = hdmi_audio_infoframe_init(&frame);
	if (ret < 0)
		return ret;

	frame.coding_type = HDMI_AUDIO_CODING_TYPE_STREAM;
	frame.sample_frequency = HDMI_AUDIO_SAMPLE_FREQUENCY_STREAM;
	frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_STREAM;
	frame.channels = audio->channels;

	ret = hdmi_audio_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (ret < 0)
		return ret;

	regmap_write(dp->regmap, DPTX_SDP_REGISTER_BANK_N(audio->id),
		     get_unaligned_le32(&header));

	for (i = 1; i < DIV_ROUND_UP(size, 4); i++) {
		size_t num = min_t(size_t, size - i * 4, 4);
		u32 value = 0;

		for (j = 0; j < num; j++)
			value |= buffer[i * 4 + j] << (j * 8);

		regmap_write(dp->regmap, DPTX_SDP_REGISTER_BANK_N(audio->id) + 4 * i, value);
	}

	regmap_update_bits(dp->regmap, DPTX_SDP_VERTICAL_CTRL_N(audio->id),
			   EN_VERTICAL_SDP, FIELD_PREP(EN_VERTICAL_SDP, 1));

	return 0;
}

static int dw_dp_audio_startup(struct device *dev, void *data)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct dw_dp_audio *audio = (struct dw_dp_audio *)data;
	int ret = 0;

	pm_runtime_get_sync(dp->dev);
	regmap_update_bits(dp->regmap, DPTX_SDP_VERTICAL_CTRL_N(audio->id),
			   EN_AUDIO_STREAM_SDP | EN_AUDIO_TIMESTAMP_SDP,
			   FIELD_PREP(EN_AUDIO_STREAM_SDP, 1) |
			   FIELD_PREP(EN_AUDIO_TIMESTAMP_SDP, 1));
	regmap_update_bits(dp->regmap, DPTX_SDP_HORIZONTAL_CTRL_N(audio->id),
			   EN_AUDIO_STREAM_SDP,
			   FIELD_PREP(EN_AUDIO_STREAM_SDP, 1));
	ret =  dw_dp_audio_infoframe_send(dp, audio);
	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);

	return ret;
}

static void dw_dp_audio_shutdown(struct device *dev, void *data)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct dw_dp_audio *audio = (struct dw_dp_audio *)data;

	pm_runtime_get_sync(dp->dev);
	regmap_update_bits(dp->regmap, DPTX_AUD_CONFIG1_N(audio->id), AUDIO_DATA_IN_EN,
			   FIELD_PREP(AUDIO_DATA_IN_EN, 0));
	pm_runtime_mark_last_busy(dp->dev);
	pm_runtime_put_autosuspend(dp->dev);

	if (audio->format == AFMT_SPDIF)
		clk_disable_unprepare(audio->spdif_clk);
	else if (audio->format == AFMT_I2S)
		clk_disable_unprepare(audio->i2s_clk);

	audio->format = AFMT_UNUSED;
}

static int dw_dp_audio_hook_plugged_cb(struct device *dev, void *data,
				       hdmi_codec_plugged_cb fn,
				       struct device *codec_dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct dw_dp_audio *audio = (struct dw_dp_audio *)data;

	audio->plugged_cb = fn;
	audio->codec_dev = codec_dev;

	if (!dp->is_mst && !audio->id)
		dw_dp_audio_handle_plugged_change(audio, dw_dp_detect(dp));
	else
		dw_dp_audio_handle_plugged_change(audio, dp->mst_enc[audio->id].active);

	return 0;
}

static int dw_dp_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
			       size_t len)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct dw_dp_audio *audio = (struct dw_dp_audio *)data;
	struct dw_dp_mst_conn *mst_conn;
	struct drm_connector *connector;

	if (!dp->is_mst && !audio->id) {
		connector = &dp->connector;
		memcpy(buf, connector->eld, min(sizeof(connector->eld), len));
	} else {
		mst_conn = dp->mst_enc[audio->id].mst_conn;
		if (mst_conn)
			memcpy(buf, mst_conn->connector.eld,
			       min(sizeof(mst_conn->connector.eld), len));
		else
			memset(buf, 0, sizeof(mst_conn->connector.eld));
	}

	return 0;
}

static const struct hdmi_codec_ops dw_dp_audio_codec_ops = {
	.hw_params = dw_dp_audio_hw_params,
	.audio_startup = dw_dp_audio_startup,
	.audio_shutdown = dw_dp_audio_shutdown,
	.get_eld = dw_dp_audio_get_eld,
	.hook_plugged_cb = dw_dp_audio_hook_plugged_cb
};

static int dw_dp_register_audio_driver(struct dw_dp *dp, struct dw_dp_audio *audio)
{
	struct hdmi_codec_pdata codec_data = {
		.ops = &dw_dp_audio_codec_ops,
		.spdif = 1,
		.i2s = 1,
		.max_i2s_channels = 8,
		.data = audio,
	};
	struct platform_device_info pdevinfo = {
		.parent = dp->dev,
		.fwnode = dp->support_mst ? of_fwnode_handle(dp->mst_enc[audio->id].port_node) :
			  NULL,
		.name = HDMI_CODEC_DRV_NAME,
		.id = PLATFORM_DEVID_AUTO,
		.data = &codec_data,
		.size_data = sizeof(codec_data),
	};

	audio->format = AFMT_UNUSED;
	audio->pdev = platform_device_register_full(&pdevinfo);

	return PTR_ERR_OR_ZERO(audio->pdev);
}

static void dw_dp_unregister_audio_driver(void *data)
{
	struct dw_dp_audio *audio = data;

	if (audio->pdev) {
		platform_device_unregister(audio->pdev);
		audio->pdev = NULL;
	}
}

static int dw_dp_single_audio_init(struct dw_dp *dp, struct dw_dp_audio *audio)
{
	int ret;

	audio->extcon = devm_extcon_dev_allocate(dp->dev, dw_dp_cable);
		if (IS_ERR(audio->extcon))
			return dev_err_probe(dp->dev, PTR_ERR(audio->extcon),
			       "failed to allocate extcon device\n");

	ret = devm_extcon_dev_register(dp->dev, audio->extcon);
	if (ret)
		return dev_err_probe(dp->dev, ret, "failed to register extcon device\n");

	ret = dw_dp_register_audio_driver(dp, audio);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dp->dev, dw_dp_unregister_audio_driver, audio);
	if (ret)
		return ret;

	return 0;
}

static int dw_dp_audio_init(struct dw_dp *dp)
{
	struct dw_dp_audio *audio;
	int i, ret;

	audio = dp->audio;
	ret = dw_dp_single_audio_init(dp, audio);
	if (ret)
		return ret;

	if (!dp->support_mst)
		return 0;

	for (i = 1; i < dp->mst_port_num; i++) {
		if (!of_device_is_available(dp->mst_enc[i].port_node))
			continue;

		audio = dp->mst_enc[i].audio;
		ret = dw_dp_single_audio_init(dp, audio);
		if (ret)
			return ret;
	}

	return 0;
}

static int dw_dp_get_audio_clk(struct dw_dp *dp)
{
	struct dw_dp_audio *audio;
	char clk_name[10];
	int i;

	audio = devm_kzalloc(dp->dev, sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	audio->id = 0;
	dp->audio = audio;

	audio->i2s_clk = devm_clk_get_optional(dp->dev, "i2s");
	if (IS_ERR(audio->i2s_clk))
		return dev_err_probe(dp->dev, PTR_ERR(audio->i2s_clk),
				     "failed to get i2s clock\n");

	audio->spdif_clk = devm_clk_get_optional(dp->dev, "spdif");
	if (IS_ERR(audio->spdif_clk))
		return dev_err_probe(dp->dev, PTR_ERR(audio->spdif_clk),
				     "failed to get spdif clock\n");

	if (!dp->support_mst)
		return 0;

	dp->mst_enc[0].audio = audio;

	for (i = 1; i < dp->mst_port_num; i++) {
		if (!of_device_is_available(dp->mst_enc[i].port_node))
			continue;

		audio = devm_kzalloc(dp->dev, sizeof(*audio), GFP_KERNEL);
		if (!audio)
			return -ENOMEM;

		audio->id = i;

		snprintf(clk_name, sizeof(clk_name), "i2s%d", i);
		audio->i2s_clk = devm_clk_get_optional(dp->dev, clk_name);
		if (IS_ERR(audio->i2s_clk))
			return dev_err_probe(dp->dev, PTR_ERR(audio->i2s_clk),
					     "failed to get i2s clock\n");

		snprintf(clk_name, sizeof(clk_name), "spdif%d", i);
		audio->spdif_clk = devm_clk_get_optional(dp->dev, clk_name);
		if (IS_ERR(audio->spdif_clk))
			return dev_err_probe(dp->dev, PTR_ERR(audio->spdif_clk),
					     "failed to get spdif clock\n");
		dp->mst_enc[i].audio = audio;
	}

	return 0;
}

static int dw_dp_encoder_late_register(struct drm_encoder *encoder)
{
	struct dw_dp *dp = encoder_to_dp(encoder);
	int ret;

	ret = dw_dp_audio_init(dp);
	if (ret)
		dev_warn(dp->dev, "audio init failed\n");

	return 0;
}

static const struct drm_encoder_funcs dw_dp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
	.late_register = dw_dp_encoder_late_register,
};

static void dw_dp_mst_poll_hpd_irq(void *data)
{
	struct dw_dp *dp = data;

	mutex_lock(&dp->irq_lock);
	dp->hotplug.long_hpd = false;
	mutex_unlock(&dp->irq_lock);

	schedule_work(&dp->hpd_work);
}

static int dw_dp_bind(struct device *dev, struct device *master, void *data)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_bridge *bridge = &dp->bridge;
	int ret;

	dp->aux.dev = dev;
	dp->aux.drm_dev = drm_dev;
	dp->aux.name = dev_name(dev);
	dp->aux.transfer = dw_dp_aux_transfer;
	ret = drm_dp_aux_register(&dp->aux);
	if (ret)
		return ret;

	if (!dp->left) {
		struct device_node *port_node;

		if (dp->support_mst)
			port_node = dp->mst_enc[0].port_node;
		else
			port_node = dev->of_node;
		drm_encoder_init(drm_dev, encoder, &dw_dp_encoder_funcs,
				 DRM_MODE_ENCODER_TMDS, NULL);
		drm_encoder_helper_add(encoder, &dw_dp_encoder_helper_funcs);

		encoder->possible_crtcs =
			rockchip_drm_of_find_possible_crtcs(drm_dev, port_node);

		ret = drm_bridge_attach(encoder, bridge, NULL, 0);
		if (ret) {
			dev_err(dev, "failed to attach bridge: %d\n", ret);
			goto error_unregister_aux;
		}
	}

	if (dp->right) {
		struct dw_dp *secondary = dp->right;
		struct drm_bridge *last_bridge =
			list_last_entry(&encoder->bridge_chain,
					struct drm_bridge, chain_node);

		ret = drm_bridge_attach(encoder, &secondary->bridge, last_bridge,
					DRM_BRIDGE_ATTACH_NO_CONNECTOR);
		if (ret)
			goto error_unregister_aux;
	}

	if (dp->aux_client) {
		dp->aux_client->register_hpd_irq(dp->aux_client, dw_dp_mst_poll_hpd_irq, dp);
		dp->aux_client->register_transfer(dp->aux_client, dw_dp_aux_transfer);
		dp->aux.transfer = dw_dp_sim_aux_transfer;
	}

	pm_runtime_use_autosuspend(dp->dev);
	pm_runtime_set_autosuspend_delay(dp->dev, 500);
	pm_runtime_enable(dp->dev);
	if (!dp->dynamic_pd_ctrl)
		pm_runtime_get_sync(dp->dev);
	else
		phy_set_mode_ext(dp->phy, PHY_MODE_DP, 1);

	enable_irq(dp->irq);
	if (dp->hpd_gpio)
		enable_irq(dp->hpd_irq);

	return 0;

error_unregister_aux:
	drm_dp_aux_unregister(&dp->aux);
	return ret;
}

static void dw_dp_unbind(struct device *dev, struct device *master, void *data)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	drm_dp_aux_unregister(&dp->aux);

	if (dp->hpd_gpio)
		disable_irq(dp->hpd_irq);
	disable_irq(dp->irq);

	if (!dp->dynamic_pd_ctrl)
		pm_runtime_put(dp->dev);
	pm_runtime_dont_use_autosuspend(dp->dev);
	pm_runtime_disable(dp->dev);

	drm_encoder_cleanup(&dp->encoder);
}

static const struct component_ops dw_dp_component_ops = {
	.bind = dw_dp_bind,
	.unbind = dw_dp_unbind,
};

static const struct regmap_range dw_dp_readable_ranges[] = {
	regmap_reg_range(DPTX_VERSION_NUMBER, DPTX_ID),
	regmap_reg_range(DPTX_CONFIG_REG1, DPTX_CONFIG_REG3),
	regmap_reg_range(DPTX_CCTL, DPTX_MST_VCP_TABLE_REG_N(7)),
	regmap_reg_range(DPTX_VSAMPLE_CTRL_N(0), DPTX_VIDEO_HBLANK_INTERVAL_N(0)),
	regmap_reg_range(DPTX_AUD_CONFIG1_N(0), DPTX_AUD_CONFIG1_N(0)),
	regmap_reg_range(DPTX_SDP_VERTICAL_CTRL_N(0), DPTX_SDP_STATUS_EN_N(0)),
	regmap_reg_range(DPTX_PHYIF_CTRL, DPTX_PHYIF_PWRDOWN_CTRL),
	regmap_reg_range(DPTX_AUX_CMD, DPTX_AUX_DATA3),
	regmap_reg_range(DPTX_GENERAL_INTERRUPT, DPTX_HPD_INTERRUPT_ENABLE),
	regmap_reg_range(DPTX_HDCPCFG, DPTX_HDCPKSVMEMCTRL),
	regmap_reg_range(DPTX_HDCPREG_BKSV0, DPTX_HDCPREG_DPK_CRC),
	regmap_reg_range(DPTX_VSAMPLE_CTRL_N(1), DPTX_VIDEO_HBLANK_INTERVAL_N(1)),
	regmap_reg_range(DPTX_AUD_CONFIG1_N(1), DPTX_AUD_CONFIG1_N(1)),
	regmap_reg_range(DPTX_SDP_VERTICAL_CTRL_N(1), DPTX_SDP_STATUS_EN_N(1)),
	regmap_reg_range(DPTX_VSAMPLE_CTRL_N(2), DPTX_VIDEO_HBLANK_INTERVAL_N(2)),
	regmap_reg_range(DPTX_AUD_CONFIG1_N(2), DPTX_AUD_CONFIG1_N(2)),
	regmap_reg_range(DPTX_SDP_VERTICAL_CTRL_N(2), DPTX_SDP_STATUS_EN_N(2)),
	regmap_reg_range(DPTX_VSAMPLE_CTRL_N(3), DPTX_VIDEO_HBLANK_INTERVAL_N(3)),
	regmap_reg_range(DPTX_AUD_CONFIG1_N(3), DPTX_AUD_CONFIG1_N(3)),
	regmap_reg_range(DPTX_SDP_VERTICAL_CTRL_N(3), DPTX_SDP_STATUS_EN_N(3)),
};

static const struct regmap_access_table dw_dp_readable_table = {
	.yes_ranges     = dw_dp_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(dw_dp_readable_ranges),
};

static const struct regmap_config dw_dp_rk3576_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.fast_io = true,
	.max_register = DPTX_MAX_REGISTER + 0x10000 * 2,
	.rd_table = &dw_dp_readable_table,
};

static const struct regmap_config dw_dp_rk3588_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.fast_io = true,
	.max_register = DPTX_MAX_REGISTER,
	.rd_table = &dw_dp_readable_table,
};

static u32 dw_dp_parse_link_frequencies(struct dw_dp *dp)
{
	struct device_node *node = dp->dev->of_node;
	struct device_node *endpoint;
	u64 frequency = 0;
	int cnt;

	endpoint = of_graph_get_endpoint_by_regs(node, 1, 0);
	if (!endpoint)
		return 0;

	cnt = of_property_count_u64_elems(endpoint, "link-frequencies");
	if (cnt > 0)
		of_property_read_u64_index(endpoint, "link-frequencies",
					   cnt - 1, &frequency);
	of_node_put(endpoint);

	if (!frequency)
		return 0;

	do_div(frequency, 10 * 1000);	/* symbol rate kbytes */

	switch (frequency) {
	case 162000:
	case 270000:
	case 540000:
	case 810000:
		break;
	default:
		dev_err(dp->dev, "invalid link frequency value: %llu\n", frequency);
		return 0;
	}

	return frequency;
}

static int dw_dp_parse_dt(struct dw_dp *dp)
{
	dp->force_hpd = device_property_read_bool(dp->dev, "force-hpd");

	dp->max_link_rate = dw_dp_parse_link_frequencies(dp);
	if (!dp->max_link_rate)
		dp->max_link_rate = 810000;

	return 0;
}

static int dw_dp_get_port_node(struct dw_dp *dp)
{

	struct device_node *port_node;
	char child_name[10];
	int i;

	for (i = 0; i < dp->mst_port_num; i++) {
		snprintf(child_name, sizeof(child_name), "dp%d", i);
		port_node = of_get_child_by_name(dp->dev->of_node, child_name);
		if (!port_node)
			return -EINVAL;
		dp->mst_enc[i].port_node = port_node;
	}

	if (!of_device_is_available(dp->mst_enc[0].port_node))
		return -EINVAL;

	return 0;
}

static int dw_dp_typec_mux_set(struct typec_mux_dev *mux, struct typec_mux_state *state)
{
	struct dw_dp *dp = typec_mux_get_drvdata(mux);

	mutex_lock(&dp->irq_lock);
	if (state->alt && state->alt->svid == USB_TYPEC_DP_SID) {
		struct typec_displayport_data *data = state->data;

		if (!data) {
			dev_dbg(dp->dev, "hotunplug from the usbdp\n");
			dp->hotplug.long_hpd = true;
			dp->hotplug.status = false;
			schedule_work(&dp->hpd_work);
		} else if (data->status & DP_STATUS_IRQ_HPD) {
			dev_dbg(dp->dev, "IRQ from the usbdp\n");
			dp->hotplug.long_hpd = false;
			dp->hotplug.status = true;
			schedule_work(&dp->hpd_work);
		} else if (data->status & DP_STATUS_HPD_STATE) {
			dev_dbg(dp->dev, "hotplug from the usbdp\n");
			dp->hotplug.long_hpd = true;
			dp->hotplug.status = true;
			schedule_work(&dp->hpd_work);
		} else {
			dev_dbg(dp->dev, "hotunplug from the usbdp\n");
			dp->hotplug.long_hpd = true;
			dp->hotplug.status = false;
			schedule_work(&dp->hpd_work);
		}
	}
	mutex_unlock(&dp->irq_lock);

	return 0;
}

static int dw_dp_setup_typec_mux(struct dw_dp *dp)
{
	struct typec_mux_desc mux_desc = {};

	mux_desc.drvdata = dp;
	mux_desc.fwnode = dev_fwnode(dp->dev);
	mux_desc.set = dw_dp_typec_mux_set;

	dp->mux = typec_mux_register(dp->dev, &mux_desc);
	if (IS_ERR(dp->mux)) {
		dev_err(dp->dev, "Error register typec mux: %ld\n", PTR_ERR(dp->mux));
		return PTR_ERR(dp->mux);
	}

	return 0;
}

static void dw_dp_typec_mux_unregister(void *data)
{
	struct dw_dp *dp = data;

	typec_mux_unregister(dp->mux);
}

static int dw_dp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_dp *dp;
	const struct dw_dp_chip_data *dp_data;
	const struct regmap_config *config;
	void __iomem *base;
	int id, ret;

	dp = devm_kzalloc(dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	id = of_alias_get_id(dev->of_node, "dp");
	if (id < 0)
		id = 0;

	dp->id = id;
	dp->dev = dev;

	dp_data = of_device_get_match_data(dev);
	if (!dp_data)
		return -ENODEV;
	dp->chip_data = dp_data;
	dp->support_mst = dp_data[dp->id].mst;
	dp->mst_port_num = dp_data[dp->id].mst_port_num;
	dp->pixel_mode = dp_data[dp->id].pixel_mode;

	ret = dw_dp_parse_dt(dp);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse DT\n");

	if (dp->support_mst) {
		ret = dw_dp_get_port_node(dp);
		if (ret) {
			dev_err(dev, "failed to parse mst dp node\n");
			return ret;
		}
	}

	mutex_init(&dp->irq_lock);
	INIT_WORK(&dp->hpd_work, dw_dp_hpd_work);
	INIT_DELAYED_WORK(&dp->hotplug.state_work, dw_dp_gpio_hpd_state_work);
	init_completion(&dp->complete);
	init_completion(&dp->hdcp_complete);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	switch (dp_data[dp->id].chip_type) {
	case RK3576_DP:
		config = &dw_dp_rk3576_regmap_config;
		break;
	case RK3588_DP:
	default:
		config = &dw_dp_rk3588_regmap_config;
		break;
	}

	dp->regmap = devm_regmap_init_mmio(dev, base, config);
	if (IS_ERR(dp->regmap))
		return dev_err_probe(dev, PTR_ERR(dp->regmap),
				     "failed to create regmap\n");

	dp->phy = devm_of_phy_get(dev, dev->of_node, NULL);
	if (IS_ERR(dp->phy))
		return dev_err_probe(dev, PTR_ERR(dp->phy),
				     "failed to get phy\n");

	dp->apb_clk = devm_clk_get(dev, "apb");
	if (IS_ERR(dp->apb_clk))
		return dev_err_probe(dev, PTR_ERR(dp->apb_clk),
				     "failed to get apb clock\n");

	dp->aux_clk = devm_clk_get(dev, "aux");
	if (IS_ERR(dp->aux_clk))
		return dev_err_probe(dev, PTR_ERR(dp->aux_clk),
				     "failed to get aux clock\n");

	dp->hclk = devm_clk_get_optional(dev, "hclk");
	if (IS_ERR(dp->hclk))
		return dev_err_probe(dev, PTR_ERR(dp->hclk),
				     "failed to get hclk\n");

	dp->hdcp_clk = devm_clk_get(dev, "hdcp");
	if (IS_ERR(dp->hdcp_clk))
		return dev_err_probe(dev, PTR_ERR(dp->hdcp_clk),
				     "failed to get hdcp clock\n");

	dp->rstc = devm_reset_control_get(dev, NULL);
	if (IS_ERR(dp->rstc))
		return dev_err_probe(dev, PTR_ERR(dp->rstc),
				     "failed to get reset control\n");

	dp->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
	if (IS_ERR(dp->hpd_gpio))
		return dev_err_probe(dev, PTR_ERR(dp->hpd_gpio),
				     "failed to get hpd GPIO\n");
	if (dp->hpd_gpio) {
		dp->hpd_irq = gpiod_to_irq(dp->hpd_gpio);
		if (dp->hpd_irq < 0)
			return dev_err_probe(dev, dp->hpd_irq,
					     "failed to get hpd irq\n");

		irq_set_status_flags(dp->hpd_irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, dp->hpd_irq, NULL,
						dw_dp_hpd_irq_handler,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT, "dw-dp-hpd", dp);
		if (ret) {
			dev_err(dev, "failed to request HPD interrupt\n");
			return ret;
		}
	}

	ret = dw_dp_get_audio_clk(dp);
	if (ret)
		return ret;

	if (device_property_present(dev, "svid")) {
		ret = dw_dp_setup_typec_mux(dp);
		if (ret)
			return ret;

		ret = devm_add_action_or_reset(dev, dw_dp_typec_mux_unregister, dp);
		if (ret)
			return ret;
		dp->usbdp_hpd = true;
	}

	if (dp->hpd_gpio || dp->force_hpd || dp->usbdp_hpd)
		dp->dynamic_pd_ctrl = true;

	dp->irq = platform_get_irq(pdev, 0);
	if (dp->irq < 0)
		return dp->irq;

	irq_set_status_flags(dp->irq, IRQ_NOAUTOEN);
	ret = devm_request_threaded_irq(dev, dp->irq, NULL, dw_dp_irq_handler,
					IRQF_ONESHOT, dev_name(dev), dp);
	if (ret) {
		dev_err(dev, "failed to request irq: %d\n", ret);
		return ret;
	}

	dp->aux_client = rockchip_dp_get_aux_client(dev->of_node, "rockchip,mst-sim");
	if (IS_ERR(dp->aux_client))
		return dev_err_probe(dev, PTR_ERR(dp->aux_client),
				     "failed to get dp aux_client\n");

	dp->bridge.of_node = dp->support_mst ? dp->mst_enc[0].port_node : dev->of_node;
	dp->bridge.funcs = &dw_dp_bridge_funcs;
	dp->bridge.ops = DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID |
			 DRM_BRIDGE_OP_HPD;
	dp->bridge.type = DRM_MODE_CONNECTOR_DisplayPort;

	platform_set_drvdata(pdev, dp);

	if (device_property_read_bool(dev, "split-mode") ||
	    device_property_read_bool(dev, "rockchip,split-mode")) {
		struct dw_dp *secondary = dw_dp_find_by_id(dev->driver, !dp->id);

		if (!secondary)
			return -EPROBE_DEFER;

		dp->right = secondary;
		dp->split_mode = true;
		secondary->left = dp;
		secondary->split_mode = true;
	}

	if (device_property_read_bool(dev, "rockchip,dual-connector-split")) {
		dp->dual_connector_split = true;

		if (device_property_read_bool(dev, "rockchip,left-display"))
			dp->left_display = true;
	}

	dw_dp_hdcp_init(dp);

	return component_add(dev, &dw_dp_component_ops);
}

static int dw_dp_remove(struct platform_device *pdev)
{
	struct dw_dp *dp = platform_get_drvdata(pdev);

	component_del(dp->dev, &dw_dp_component_ops);
	cancel_work_sync(&dp->hpd_work);
	cancel_delayed_work_sync(&dp->hotplug.state_work);

	return 0;
}

static int dw_dp_runtime_suspend(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	clk_disable_unprepare(dp->aux_clk);
	clk_disable_unprepare(dp->apb_clk);
	clk_disable_unprepare(dp->hclk);

	return 0;
}

static int dw_dp_runtime_resume(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	clk_prepare_enable(dp->hclk);
	clk_prepare_enable(dp->apb_clk);
	clk_prepare_enable(dp->aux_clk);

	dw_dp_init(dp);

	return 0;
}

static int dw_dp_suspend_noirq(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	pm_runtime_force_suspend(dev);
	if (dp->is_mst)
		phy_power_off(dp->phy);

	return 0;
}

static int dw_dp_resume_noirq(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	pm_runtime_force_resume(dev);
	if (dp->is_mst)
		phy_power_on(dp->phy);

	return 0;
}

static int dw_dp_suspend(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);

	if (dp->is_mst)
		drm_dp_mst_topology_mgr_suspend(&dp->mst_mgr);

	return 0;
}

static int dw_dp_resume(struct device *dev)
{
	struct dw_dp *dp = dev_get_drvdata(dev);
	int ret;

	if (!dp->support_mst)
		return 0;

	ret = drm_dp_mst_topology_mgr_resume(&dp->mst_mgr, true);
	if (ret) {
		if (dp->is_mst)
			phy_power_off(dp->phy);
		dp->is_mst = false;
		drm_dp_mst_topology_mgr_set_mst(&dp->mst_mgr, false);
	}

	return 0;
}

static const struct dev_pm_ops dw_dp_pm_ops = {
	RUNTIME_PM_OPS(dw_dp_runtime_suspend, dw_dp_runtime_resume, NULL)
	NOIRQ_SYSTEM_SLEEP_PM_OPS(dw_dp_suspend_noirq, dw_dp_resume_noirq)
	SYSTEM_SLEEP_PM_OPS(dw_dp_suspend, dw_dp_resume)
};

static const struct dw_dp_chip_data rk3588_dp[] = {
	{
		.chip_type = RK3588_DP,
		.mst = false,
		.pixel_mode = DPTX_MP_QUAD_PIXEL,
	},
	{
		.chip_type = RK3588_DP,
		.mst = false,
		.pixel_mode = DPTX_MP_QUAD_PIXEL,
	},
	{ /* sentinel */ }
};

static const struct dw_dp_chip_data rk3576_dp[] = {
	{
		.chip_type = RK3576_DP,
		.mst = true,
		.mst_port_num = 3,
		.pixel_mode = DPTX_MP_DUAL_PIXEL,
		.caps = {
			{
				.max_hactive = 4096,
				.max_vactive = 2160,
				.max_pixel_clock = 1188000,
			},
			{
				.max_hactive = 2560,
				.max_vactive = 1440,
				.max_pixel_clock = 300000,
			},
			{
				.max_hactive = 1920,
				.max_vactive = 1080,
				.max_pixel_clock = 150000,
			},


		},
	},
	{ /* sentinel */ }
};

static const struct of_device_id dw_dp_of_match[] = {
	{ .compatible = "rockchip,rk3588-dp", .data = &rk3588_dp},
	{ .compatible = "rockchip,rk3576-dp", .data = &rk3576_dp},
	{}
};
MODULE_DEVICE_TABLE(of, dw_dp_of_match);

struct platform_driver dw_dp_driver = {
	.probe	= dw_dp_probe,
	.remove = dw_dp_remove,
	.driver = {
		.name = "dw-dp",
		.of_match_table = dw_dp_of_match,
		.pm = pm_ptr(&dw_dp_pm_ops),
	},
};
