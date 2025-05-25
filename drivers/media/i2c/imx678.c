// SPDX-License-Identifier: GPL-2.0
/*
 * imx678 driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 * V0.0X01.0X01
 *  1. fix hdr ae ratio error,
 *     0x3260 should be set 0x01 in normal mode,
 *     should be 0x00 in hdr mode.
 *  2. rhs1 should be 4n+1 when set hdr ae.
 * V0.0X01.0X02
 *  1. shr0 should be greater than (rsh1 + 9).
 *  2. rhs1 should be ceil to 4n + 1.
 * V0.0X01.0X03
 *  1. support 12bit HDR DOL3
 *  2. support HDR/Linear quick switch
 * V0.0X01.0X04
 * 1. support enum format info by aiq
 * V0.0X01.0X05
 * 1. fixed 10bit hdr2/hdr3 frame rate issue
 * V0.0X01.0X06
 * 1. support DOL3 10bit 20fps 1485Mbps
 * 2. fixed linkfreq error
 * V0.0X01.0X07
 * 1. fix set_fmt & ioctl get mode unmatched issue.
 * 2. need to set default vblank when change format.
 * 3. enum all supported mode mbus_code, not just cur_mode.
 * V0.0X01.0X08
 * 1. add dcphy param for hdrx2 mode.
 */

// #define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/rk-preisp.h>
#include <media/v4l2-fwnode.h>
#include <linux/of_graph.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-tb-setup.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x08)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_FREQ_1782M			891000000
#define MIPI_FREQ_1188M			594000000

#define IMX678_4LANES			4

#define IMX678_MAX_PIXEL_RATE		(MIPI_FREQ_1782M / 10 * 2 * IMX678_4LANES)
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX678_XVCLK_FREQ_37M		37125000

/* TODO: Get the real chip id from reg */
#define CHIP_ID				0x01
#define IMX678_REG_CHIP_ID		0x3022

#define IMX678_REG_CTRL_MODE		0x3000
#define IMX678_MODE_SW_STANDBY		BIT(0)
#define IMX678_MODE_STREAMING		0x0

#define IMX678_HCG_ADDR             0x3030
#define IMX678_HCG_SEL1_ADDR        0x3031
#define IMX678_HCG_SEL2_ADDR        0x3032

#define IMX678_LF_GAIN_REG_H		0x3071
#define IMX678_LF_GAIN_REG_L		0x3070

#define IMX678_SF1_GAIN_REG_H		0x3073
#define IMX678_SF1_GAIN_REG_L		0x3072

#define IMX678_SF2_GAIN_REG_H		0x3075
#define IMX678_SF2_GAIN_REG_L		0x3074

#define IMX678_LF_EXPO_REG_H		0x3052
#define IMX678_LF_EXPO_REG_M		0x3051
#define IMX678_LF_EXPO_REG_L		0x3050

#define IMX678_SF1_EXPO_REG_H		0x3056
#define IMX678_SF1_EXPO_REG_M		0x3055
#define IMX678_SF1_EXPO_REG_L		0x3054

#define IMX678_SF2_EXPO_REG_H		0x305A
#define IMX678_SF2_EXPO_REG_M		0x3059
#define IMX678_SF2_EXPO_REG_L		0x3058

#define IMX678_RHS1_REG_H		0x3062
#define IMX678_RHS1_REG_M		0x3061
#define IMX678_RHS1_REG_L		0x3060
#define IMX678_RHS1_DEFAULT		0x004D

#define IMX678_RHS2_REG_H		0x3066
#define IMX678_RHS2_REG_M		0x3065
#define IMX678_RHS2_REG_L		0x3064
#define IMX678_RHS2_DEFAULT		0x004D

#define	IMX678_EXPOSURE_MIN		4
#define	IMX678_EXPOSURE_STEP		1
#define IMX678_VTS_MAX			0x7fff

#define IMX678_GAIN_MIN			0x00
#define IMX678_GAIN_MAX			0xf0
#define IMX678_GAIN_STEP		1
#define IMX678_GAIN_DEFAULT		0x00

#define IMX678_FETCH_GAIN_H(VAL)	(((VAL) >> 8) & 0x07)
#define IMX678_FETCH_GAIN_L(VAL)	((VAL) & 0xFF)

#define IMX678_FETCH_EXP_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_EXP_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX678_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define IMX678_FETCH_VTS_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_VTS_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_VTS_L(VAL)		((VAL) & 0xFF)

#define IMX678_VTS_REG_L		0x3028
#define IMX678_VTS_REG_M		0x3029
#define IMX678_VTS_REG_H		0x302a

#define IMX678_MIRROR_BIT_MASK		BIT(0)
#define IMX678_FLIP_BIT_MASK		BIT(1)
#define IMX678_FLIP_REG			  0x3021
#define IMX678_MIRROR_REG         0x3020

#define REG_NULL			0xFFFF
#define REG_DELAY			0xFFFE

#define IMX678_REG_VALUE_08BIT		1
#define IMX678_REG_VALUE_16BIT		2
#define IMX678_REG_VALUE_24BIT		3

#define IMX678_GROUP_HOLD_REG		0x3001
#define IMX678_GROUP_HOLD_START		0x01
#define IMX678_GROUP_HOLD_END		0x00

/* Basic Readout Lines. Number of necessary readout lines in sensor */
#define BRL_ALL				2228u
#define BRL_BINNING			1115u
/* Readout timing setting of SEF1(DOL2): RHS1 < 2 * BRL and should be 4n + 1 */
#define RHS1_MAX_X2(VAL)		(((VAL) * 2 - 1) / 4 * 4 + 1)
#define SHR1_MIN_X2			9u

/* Readout timing setting of SEF1(DOL3): RHS1 < 3 * BRL and should be 6n + 1 */
#define RHS1_MAX_X3(VAL)		(((VAL) * 3 - 1) / 6 * 6 + 1)
#define SHR1_MIN_X3			13u

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define RKMODULE_CAMERA_FASTBOOT_ENABLE "rockchip,camera_fastboot"

#define IMX678_NAME			"imx678"

static const char * const imx678_supply_names[] = {
	"dvdd",		/* Digital core power */
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
};

#define IMX678_NUM_SUPPLIES ARRAY_SIZE(imx678_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct imx678_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 bpp;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
	u32 xvclk;
};

struct imx678 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*power_gpio;
	struct regulator_bulk_data supplies[IMX678_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_a_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	u32			is_thunderboot;
	bool			is_thunderboot_ng;
	bool			is_first_streamoff;
	const struct imx678_mode *supported_modes;
	const struct imx678_mode *cur_mode;
	u32			module_index;
	u32			cfg_num;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct v4l2_fwnode_endpoint bus_cfg;
	struct cam_sw_info *cam_sw_inf;
	int			rhs1_old;
	int			rhs2_old;
	u32			cur_exposure[3];
	u32			cur_gain[3];
	u32			pclk;
	u32			tline;
	bool			is_tline_init;
};

static struct rkmodule_csi_dphy_param dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,
	.lp_vol_ref = 6,
	.lp_hys_sw = {3, 0, 0, 0},
	.lp_escclk_pol_sel = {1, 1, 1, 1},
	.skew_data_cal_clk = {0, 3, 3, 3},
	.clk_hs_term_sel = 2,
	.data_hs_term_sel = {2, 2, 2, 2},
	.reserved = {0},
};

#define to_imx678(sd) container_of(sd, struct imx678, subdev)

/*
 * Xclk 37.125Mhz  datarate 1188  12bit 4lane
 */
static __maybe_unused const struct regval imx678_linear_12bit_3840x2160_1188M_regs[] = {
	{0x3000, 0x01}, //STANDBY
	{0x3002, 0x01}, //XMSTA
	{0x3014, 0x01}, //INCK_SEL[3:0]
	{0x3015, 0x04}, //DATARATE_SEL[3:0]
	{0x3018, 0x00}, //WINMODE[4:0]
	{0x301A, 0x00}, //WDMODE
	{0x301B, 0x00}, //ADDMODE[1:0]
	{0x301C, 0x00}, //THIN_V_EN
	{0x301E, 0x01}, //VCMODE
	{0x3020, 0x00}, //HREVERSE
	{0x3021, 0x00}, //VREVERSE
	{0x3022, 0x01}, //ADBIT[1:0]
	{0x3023, 0x01}, //MDBIT
	{0x3028, 0xCA}, //VMAX[19:0]
	{0x3029, 0x08}, //VMAX[19:0]
	{0x302A, 0x00}, //VMAX[19:0]
	{0x302C, 0x4C}, //HMAX[15:0]
	{0x302D, 0x04}, //HMAX[15:0]
	{0x3030, 0x00}, //FDG_SEL0[1:0]
	{0x303C, 0x00}, //PIX_HST[12:0]
	{0x303D, 0x00}, //PIX_HST[12:0]
	{0x303E, 0x10}, //PIX_HWIDTH[12:0]
	{0x303F, 0x0F}, //PIX_HWIDTH[12:0]
	{0x3040, 0x03}, //LANEMODE[2:0]
	{0x3044, 0x00}, //PIX_VST[11:0]
	{0x3045, 0x00}, //PIX_VST[11:0]
	{0x3046, 0x84}, //PIX_VWIDTH[11:0]
	{0x3047, 0x08}, //PIX_VWIDTH[11:0]
	{0x3050, 0x27}, //SHR0[19:0]
	{0x3051, 0x06}, //SHR0[19:0]
	{0x3052, 0x00}, //SHR0[19:0]
	{0x30A6, 0x00}, //XVS_DRV[1:0]
	{0x30DC, 0x32}, //BLKLEVEL[11:0]
	{0x30DD, 0x40}, //BLKLEVEL[11:0]
	{0x3460, 0x22},
	{0x355A, 0x64},
	{0x3A02, 0x7A},
	{0x3A10, 0xEC},
	{0x3A12, 0x71},
	{0x3A14, 0xDE},
	{0x3A20, 0x2B},
	{0x3A24, 0x22},
	{0x3A25, 0x25},
	{0x3A26, 0x2A},
	{0x3A27, 0x2C},
	{0x3A28, 0x39},
	{0x3A29, 0x38},
	{0x3A30, 0x04},
	{0x3A31, 0x04},
	{0x3A32, 0x03},
	{0x3A33, 0x03},
	{0x3A34, 0x09},
	{0x3A35, 0x06},
	{0x3A38, 0xCD},
	{0x3A3A, 0x4C},
	{0x3A3C, 0xB9},
	{0x3A3E, 0x30},
	{0x3A40, 0x2C},
	{0x3A42, 0x39},
	{0x3A4E, 0x00},
	{0x3A52, 0x00},
	{0x3A56, 0x00},
	{0x3A5A, 0x00},
	{0x3A5E, 0x00},
	{0x3A62, 0x00},
	{0x3A6E, 0xA0},
	{0x3A70, 0x50},
	{0x3A8C, 0x04},
	{0x3A8D, 0x03},
	{0x3A8E, 0x09},
	{0x3A90, 0x38},
	{0x3A91, 0x42},
	{0x3A92, 0x3C},
	{0x3B0E, 0xF3},
	{0x3B12, 0xE5},
	{0x3B27, 0xC0},
	{0x3B2E, 0xEF},
	{0x3B30, 0x6A},
	{0x3B32, 0xF6},
	{0x3B36, 0xE1},
	{0x3B3A, 0xE8},
	{0x3B5A, 0x17},
	{0x3B5E, 0xEF},
	{0x3B60, 0x6A},
	{0x3B62, 0xF6},
	{0x3B66, 0xE1},
	{0x3B6A, 0xE8},
	{0x3B88, 0xEC},
	{0x3B8A, 0xED},
	{0x3B94, 0x71},
	{0x3B96, 0x72},
	{0x3B98, 0xDE},
	{0x3B9A, 0xDF},
	{0x3C0F, 0x06},
	{0x3C10, 0x06},
	{0x3C11, 0x06},
	{0x3C12, 0x06},
	{0x3C13, 0x06},
	{0x3C18, 0x20},
	{0x3C3A, 0x7A},
	{0x3C40, 0xF4},
	{0x3C48, 0xE6},
	{0x3C54, 0xCE},
	{0x3C56, 0xD0},
	{0x3C6C, 0x53},
	{0x3C6E, 0x55},
	{0x3C70, 0xC0},
	{0x3C72, 0xC2},
	{0x3C7E, 0xCE},
	{0x3C8C, 0xCF},
	{0x3C8E, 0xEB},
	{0x3C98, 0x54},
	{0x3C9A, 0x70},
	{0x3C9C, 0xC1},
	{0x3C9E, 0xDD},
	{0x3CB0, 0x7A},
	{0x3CB2, 0xBA},
	{0x3CC8, 0xBC},
	{0x3CCA, 0x7C},
	{0x3CD4, 0xEA},
	{0x3CD5, 0x01},
	{0x3CD6, 0x4A},
	{0x3CD8, 0x00},
	{0x3CD9, 0x00},
	{0x3CDA, 0xFF},
	{0x3CDB, 0x03},
	{0x3CDC, 0x00},
	{0x3CDD, 0x00},
	{0x3CDE, 0xFF},
	{0x3CDF, 0x03},
	{0x3CE4, 0x4C},
	{0x3CE6, 0xEC},
	{0x3CE7, 0x01},
	{0x3CE8, 0xFF},
	{0x3CE9, 0x03},
	{0x3CEA, 0x00},
	{0x3CEB, 0x00},
	{0x3CEC, 0xFF},
	{0x3CED, 0x03},
	{0x3CEE, 0x00},
	{0x3CEF, 0x00},
	{0x3E28, 0x82},
	{0x3E2A, 0x80},
	{0x3E30, 0x85},
	{0x3E32, 0x7D},
	{0x3E5C, 0xCE},
	{0x3E5E, 0xD3},
	{0x3E70, 0x53},
	{0x3E72, 0x58},
	{0x3E74, 0xC0},
	{0x3E76, 0xC5},
	{0x3E78, 0xC0},
	{0x3E79, 0x01},
	{0x3E7A, 0xD4},
	{0x3E7B, 0x01},
	{0x3EB4, 0x0B},
	{0x3EB5, 0x02},
	{0x3EB6, 0x4D},
	{0x3EEC, 0xF3},
	{0x3EEE, 0xE7},
	{0x3F01, 0x01},
	{0x3F24, 0x10},
	{0x3F28, 0x2D},
	{0x3F2A, 0x2D},
	{0x3F2C, 0x2D},
	{0x3F2E, 0x2D},
	{0x3F30, 0x23},
	{0x3F38, 0x2D},
	{0x3F3A, 0x2D},
	{0x3F3C, 0x2D},
	{0x3F3E, 0x28},
	{0x3F40, 0x1E},
	{0x3F48, 0x2D},
	{0x3F4A, 0x2D},
	{0x4004, 0xE4},
	{0x4006, 0xFF},
	{0x4018, 0x69},
	{0x401A, 0x84},
	{0x401C, 0xD6},
	{0x401E, 0xF1},
	{0x4038, 0xDE},
	{0x403A, 0x00},
	{0x403B, 0x01},
	{0x404C, 0x63},
	{0x404E, 0x85},
	{0x4050, 0xD0},
	{0x4052, 0xF2},
	{0x4108, 0xDD},
	{0x410A, 0xF7},
	{0x411C, 0x62},
	{0x411E, 0x7C},
	{0x4120, 0xCF},
	{0x4122, 0xE9},
	{0x4138, 0xE6},
	{0x413A, 0xF1},
	{0x414C, 0x6B},
	{0x414E, 0x76},
	{0x4150, 0xD8},
	{0x4152, 0xE3},
	{0x417E, 0x03},
	{0x417F, 0x01},
	{0x4186, 0xE0},
	{0x4190, 0xF3},
	{0x4192, 0xF7},
	{0x419C, 0x78},
	{0x419E, 0x7C},
	{0x41A0, 0xE5},
	{0x41A2, 0xE9},
	{0x41C8, 0xE2},
	{0x41CA, 0xFD},
	{0x41DC, 0x67},
	{0x41DE, 0x82},
	{0x41E0, 0xD4},
	{0x41E2, 0xEF},
	{0x4200, 0xDE},
	{0x4202, 0xDA},
	{0x4218, 0x63},
	{0x421A, 0x5F},
	{0x421C, 0xD0},
	{0x421E, 0xCC},
	{0x425A, 0x82},
	{0x425C, 0xEF},
	{0x4348, 0xFE},
	{0x4349, 0x06},
	{0x4352, 0xCE},
	{0x4420, 0x0B},
	{0x4421, 0x02},
	{0x4422, 0x4D},
	{0x4426, 0xF5},
	{0x442A, 0xE7},
	{0x4432, 0xF5},
	{0x4436, 0xE7},
	{0x4466, 0xB4},
	{0x446E, 0x32},
	{0x449F, 0x1C},
	{0x44A4, 0x2C},
	{0x44A6, 0x2C},
	{0x44A8, 0x2C},
	{0x44AA, 0x2C},
	{0x44B4, 0x2C},
	{0x44B6, 0x2C},
	{0x44B8, 0x2C},
	{0x44BA, 0x2C},
	{0x44C4, 0x2C},
	{0x44C6, 0x2C},
	{0x44C8, 0x2C},
	{0x4506, 0xF3},
	{0x450E, 0xE5},
	{0x4516, 0xF3},
	{0x4522, 0xE5},
	{0x4524, 0xF3},
	{0x452C, 0xE5},
	{0x453C, 0x22},
	{0x453D, 0x1B},
	{0x453E, 0x1B},
	{0x453F, 0x15},
	{0x4540, 0x15},
	{0x4541, 0x15},
	{0x4542, 0x15},
	{0x4543, 0x15},
	{0x4544, 0x15},
	{0x4548, 0x00},
	{0x4549, 0x01},
	{0x454A, 0x01},
	{0x454B, 0x06},
	{0x454C, 0x06},
	{0x454D, 0x06},
	{0x454E, 0x06},
	{0x454F, 0x06},
	{0x4550, 0x06},
	{0x4554, 0x55},
	{0x4555, 0x02},
	{0x4556, 0x42},
	{0x4557, 0x05},
	{0x4558, 0xFD},
	{0x4559, 0x05},
	{0x455A, 0x94},
	{0x455B, 0x06},
	{0x455D, 0x06},
	{0x455E, 0x49},
	{0x455F, 0x07},
	{0x4560, 0x7F},
	{0x4561, 0x07},
	{0x4562, 0xA5},
	{0x4564, 0x55},
	{0x4565, 0x02},
	{0x4566, 0x42},
	{0x4567, 0x05},
	{0x4568, 0xFD},
	{0x4569, 0x05},
	{0x456A, 0x94},
	{0x456B, 0x06},
	{0x456D, 0x06},
	{0x456E, 0x49},
	{0x456F, 0x07},
	{0x4572, 0xA5},
	{0x460C, 0x7D},
	{0x460E, 0xB1},
	{0x4614, 0xA8},
	{0x4616, 0xB2},
	{0x461C, 0x7E},
	{0x461E, 0xA7},
	{0x4624, 0xA8},
	{0x4626, 0xB2},
	{0x462C, 0x7E},
	{0x462E, 0x8A},
	{0x4630, 0x94},
	{0x4632, 0xA7},
	{0x4634, 0xFB},
	{0x4636, 0x2F},
	{0x4638, 0x81},
	{0x4639, 0x01},
	{0x463A, 0xB5},
	{0x463B, 0x01},
	{0x463C, 0x26},
	{0x463E, 0x30},
	{0x4640, 0xAC},
	{0x4641, 0x01},
	{0x4642, 0xB6},
	{0x4643, 0x01},
	{0x4644, 0xFC},
	{0x4646, 0x25},
	{0x4648, 0x82},
	{0x4649, 0x01},
	{0x464A, 0xAB},
	{0x464B, 0x01},
	{0x464C, 0x26},
	{0x464E, 0x30},
	{0x4654, 0xFC},
	{0x4656, 0x08},
	{0x4658, 0x12},
	{0x465A, 0x25},
	{0x4662, 0xFC},
	{0x46A2, 0xFB},
	{0x46D6, 0xF3},
	{0x46E6, 0x00},
	{0x46E8, 0xFF},
	{0x46E9, 0x03},
	{0x46EC, 0x7A},
	{0x46EE, 0xE5},
	{0x46F4, 0xEE},
	{0x46F6, 0xF2},
	{0x470C, 0xFF},
	{0x470D, 0x03},
	{0x470E, 0x00},
	{0x4714, 0xE0},
	{0x4716, 0xE4},
	{0x471E, 0xED},
	{0x472E, 0x00},
	{0x4730, 0xFF},
	{0x4731, 0x03},
	{0x4734, 0x7B},
	{0x4736, 0xDF},
	{0x4754, 0x7D},
	{0x4756, 0x8B},
	{0x4758, 0x93},
	{0x475A, 0xB1},
	{0x475C, 0xFB},
	{0x475E, 0x09},
	{0x4760, 0x11},
	{0x4762, 0x2F},
	{0x4766, 0xCC},
	{0x4776, 0xCB},
	{0x477E, 0x4A},
	{0x478E, 0x49},
	{0x4794, 0x7C},
	{0x4796, 0x8F},
	{0x4798, 0xB3},
	{0x4799, 0x00},
	{0x479A, 0xCC},
	{0x479C, 0xC1},
	{0x479E, 0xCB},
	{0x47A4, 0x7D},
	{0x47A6, 0x8E},
	{0x47A8, 0xB4},
	{0x47A9, 0x00},
	{0x47AA, 0xC0},
	{0x47AC, 0xFA},
	{0x47AE, 0x0D},
	{0x47B0, 0x31},
	{0x47B1, 0x01},
	{0x47B2, 0x4A},
	{0x47B3, 0x01},
	{0x47B4, 0x3F},
	{0x47B6, 0x49},
	{0x47BC, 0xFB},
	{0x47BE, 0x0C},
	{0x47C0, 0x32},
	{0x47C1, 0x01},
	{0x47C2, 0x3E},
	{0x47C3, 0x01},
	{0x3002, 0x00},
	{REG_DELAY, 0x1E},//wait_ms(30)
	{REG_NULL, 0x00},
};

/*
 * IMX678LQJ
 * All-pixel scan CSI-2_4lane 37.125MHz
 * AD:10bit Output:10bit 1782Mbps
 * Master Mode LCG Mode DOL HDR 2frame VC 30fps
 * Integration Time LEF:24ms SEF:1.007ms
 */
static __maybe_unused const struct regval imx678_hdr2_10bit_3840x2160_1782M_regs[] = {
	{0x3000, 0x01},
	{0x3002, 0x01},
	{0x3014, 0x01},
	{0x3015, 0x02},
	{0x301A, 0x01},
	{0x301C, 0x01},
	{0x3022, 0x01},
	{0x3023, 0x01},
	{0x302C, 0x26},
	{0x302D, 0x02},
	{0x3050, 0xEC},
	{0x3051, 0x04},
	{0x3054, 0x05},
	{0x3055, 0x00},
	{0x3060, 0x8D},
	{0x3061, 0x00},
	{0x30A6, 0x00},
	{0x3400, 0x00},
	{0x3460, 0x22},
	{0x355A, 0x64},
	{0x3A02, 0x7A},
	{0x3A10, 0xEC},
	{0x3A12, 0x71},
	{0x3A14, 0xDE},
	{0x3A20, 0x2B},
	{0x3A24, 0x22},
	{0x3A25, 0x25},
	{0x3A26, 0x2A},
	{0x3A27, 0x2C},
	{0x3A28, 0x39},
	{0x3A29, 0x38},
	{0x3A30, 0x04},
	{0x3A31, 0x04},
	{0x3A32, 0x03},
	{0x3A33, 0x03},
	{0x3A34, 0x09},
	{0x3A35, 0x06},
	{0x3A38, 0xCD},
	{0x3A3A, 0x4C},
	{0x3A3C, 0xB9},
	{0x3A3E, 0x30},
	{0x3A40, 0x2C},
	{0x3A42, 0x39},
	{0x3A4E, 0x00},
	{0x3A52, 0x00},
	{0x3A56, 0x00},
	{0x3A5A, 0x00},
	{0x3A5E, 0x00},
	{0x3A62, 0x00},
	{0x3A6E, 0xA0},
	{0x3A70, 0x50},
	{0x3A8C, 0x04},
	{0x3A8D, 0x03},
	{0x3A8E, 0x09},
	{0x3A90, 0x38},
	{0x3A91, 0x42},
	{0x3A92, 0x3C},
	{0x3B0E, 0xF3},
	{0x3B12, 0xE5},
	{0x3B27, 0xC0},
	{0x3B2E, 0xEF},
	{0x3B30, 0x6A},
	{0x3B32, 0xF6},
	{0x3B36, 0xE1},
	{0x3B3A, 0xE8},
	{0x3B5A, 0x17},
	{0x3B5E, 0xEF},
	{0x3B60, 0x6A},
	{0x3B62, 0xF6},
	{0x3B66, 0xE1},
	{0x3B6A, 0xE8},
	{0x3B88, 0xEC},
	{0x3B8A, 0xED},
	{0x3B94, 0x71},
	{0x3B96, 0x72},
	{0x3B98, 0xDE},
	{0x3B9A, 0xDF},
	{0x3C0F, 0x06},
	{0x3C10, 0x06},
	{0x3C11, 0x06},
	{0x3C12, 0x06},
	{0x3C13, 0x06},
	{0x3C18, 0x20},
	{0x3C3A, 0x7A},
	{0x3C40, 0xF4},
	{0x3C48, 0xE6},
	{0x3C54, 0xCE},
	{0x3C56, 0xD0},
	{0x3C6C, 0x53},
	{0x3C6E, 0x55},
	{0x3C70, 0xC0},
	{0x3C72, 0xC2},
	{0x3C7E, 0xCE},
	{0x3C8C, 0xCF},
	{0x3C8E, 0xEB},
	{0x3C98, 0x54},
	{0x3C9A, 0x70},
	{0x3C9C, 0xC1},
	{0x3C9E, 0xDD},
	{0x3CB0, 0x7A},
	{0x3CB2, 0xBA},
	{0x3CC8, 0xBC},
	{0x3CCA, 0x7C},
	{0x3CD4, 0xEA},
	{0x3CD5, 0x01},
	{0x3CD6, 0x4A},
	{0x3CD8, 0x00},
	{0x3CD9, 0x00},
	{0x3CDA, 0xFF},
	{0x3CDB, 0x03},
	{0x3CDC, 0x00},
	{0x3CDD, 0x00},
	{0x3CDE, 0xFF},
	{0x3CDF, 0x03},
	{0x3CE4, 0x4C},
	{0x3CE6, 0xEC},
	{0x3CE7, 0x01},
	{0x3CE8, 0xFF},
	{0x3CE9, 0x03},
	{0x3CEA, 0x00},
	{0x3CEB, 0x00},
	{0x3CEC, 0xFF},
	{0x3CED, 0x03},
	{0x3CEE, 0x00},
	{0x3CEF, 0x00},
	{0x3E28, 0x82},
	{0x3E2A, 0x80},
	{0x3E30, 0x85},
	{0x3E32, 0x7D},
	{0x3E5C, 0xCE},
	{0x3E5E, 0xD3},
	{0x3E70, 0x53},
	{0x3E72, 0x58},
	{0x3E74, 0xC0},
	{0x3E76, 0xC5},
	{0x3E78, 0xC0},
	{0x3E79, 0x01},
	{0x3E7A, 0xD4},
	{0x3E7B, 0x01},
	{0x3EB4, 0x0B},
	{0x3EB5, 0x02},
	{0x3EB6, 0x4D},
	{0x3EEC, 0xF3},
	{0x3EEE, 0xE7},
	{0x3F01, 0x01},
	{0x3F24, 0x10},
	{0x3F28, 0x2D},
	{0x3F2A, 0x2D},
	{0x3F2C, 0x2D},
	{0x3F2E, 0x2D},
	{0x3F30, 0x23},
	{0x3F38, 0x2D},
	{0x3F3A, 0x2D},
	{0x3F3C, 0x2D},
	{0x3F3E, 0x28},
	{0x3F40, 0x1E},
	{0x3F48, 0x2D},
	{0x3F4A, 0x2D},
	{0x4004, 0xE4},
	{0x4006, 0xFF},
	{0x4018, 0x69},
	{0x401A, 0x84},
	{0x401C, 0xD6},
	{0x401E, 0xF1},
	{0x4038, 0xDE},
	{0x403A, 0x00},
	{0x403B, 0x01},
	{0x404C, 0x63},
	{0x404E, 0x85},
	{0x4050, 0xD0},
	{0x4052, 0xF2},
	{0x4108, 0xDD},
	{0x410A, 0xF7},
	{0x411C, 0x62},
	{0x411E, 0x7C},
	{0x4120, 0xCF},
	{0x4122, 0xE9},
	{0x4138, 0xE6},
	{0x413A, 0xF1},
	{0x414C, 0x6B},
	{0x414E, 0x76},
	{0x4150, 0xD8},
	{0x4152, 0xE3},
	{0x417E, 0x03},
	{0x417F, 0x01},
	{0x4186, 0xE0},
	{0x4190, 0xF3},
	{0x4192, 0xF7},
	{0x419C, 0x78},
	{0x419E, 0x7C},
	{0x41A0, 0xE5},
	{0x41A2, 0xE9},
	{0x41C8, 0xE2},
	{0x41CA, 0xFD},
	{0x41DC, 0x67},
	{0x41DE, 0x82},
	{0x41E0, 0xD4},
	{0x41E2, 0xEF},
	{0x4200, 0xDE},
	{0x4202, 0xDA},
	{0x4218, 0x63},
	{0x421A, 0x5F},
	{0x421C, 0xD0},
	{0x421E, 0xCC},
	{0x425A, 0x82},
	{0x425C, 0xEF},
	{0x4348, 0xFE},
	{0x4349, 0x06},
	{0x4352, 0xCE},
	{0x4420, 0x0B},
	{0x4421, 0x02},
	{0x4422, 0x4D},
	{0x4426, 0xF5},
	{0x442A, 0xE7},
	{0x4432, 0xF5},
	{0x4436, 0xE7},
	{0x4466, 0xB4},
	{0x446E, 0x32},
	{0x449F, 0x1C},
	{0x44A4, 0x2C},
	{0x44A6, 0x2C},
	{0x44A8, 0x2C},
	{0x44AA, 0x2C},
	{0x44B4, 0x2C},
	{0x44B6, 0x2C},
	{0x44B8, 0x2C},
	{0x44BA, 0x2C},
	{0x44C4, 0x2C},
	{0x44C6, 0x2C},
	{0x44C8, 0x2C},
	{0x4506, 0xF3},
	{0x450E, 0xE5},
	{0x4516, 0xF3},
	{0x4522, 0xE5},
	{0x4524, 0xF3},
	{0x452C, 0xE5},
	{0x453C, 0x22},
	{0x453D, 0x1B},
	{0x453E, 0x1B},
	{0x453F, 0x15},
	{0x4540, 0x15},
	{0x4541, 0x15},
	{0x4542, 0x15},
	{0x4543, 0x15},
	{0x4544, 0x15},
	{0x4548, 0x00},
	{0x4549, 0x01},
	{0x454A, 0x01},
	{0x454B, 0x06},
	{0x454C, 0x06},
	{0x454D, 0x06},
	{0x454E, 0x06},
	{0x454F, 0x06},
	{0x4550, 0x06},
	{0x4554, 0x55},
	{0x4555, 0x02},
	{0x4556, 0x42},
	{0x4557, 0x05},
	{0x4558, 0xFD},
	{0x4559, 0x05},
	{0x455A, 0x94},
	{0x455B, 0x06},
	{0x455D, 0x06},
	{0x455E, 0x49},
	{0x455F, 0x07},
	{0x4560, 0x7F},
	{0x4561, 0x07},
	{0x4562, 0xA5},
	{0x4564, 0x55},
	{0x4565, 0x02},
	{0x4566, 0x42},
	{0x4567, 0x05},
	{0x4568, 0xFD},
	{0x4569, 0x05},
	{0x456A, 0x94},
	{0x456B, 0x06},
	{0x456D, 0x06},
	{0x456E, 0x49},
	{0x456F, 0x07},
	{0x4572, 0xA5},
	{0x460C, 0x7D},
	{0x460E, 0xB1},
	{0x4614, 0xA8},
	{0x4616, 0xB2},
	{0x461C, 0x7E},
	{0x461E, 0xA7},
	{0x4624, 0xA8},
	{0x4626, 0xB2},
	{0x462C, 0x7E},
	{0x462E, 0x8A},
	{0x4630, 0x94},
	{0x4632, 0xA7},
	{0x4634, 0xFB},
	{0x4636, 0x2F},
	{0x4638, 0x81},
	{0x4639, 0x01},
	{0x463A, 0xB5},
	{0x463B, 0x01},
	{0x463C, 0x26},
	{0x463E, 0x30},
	{0x4640, 0xAC},
	{0x4641, 0x01},
	{0x4642, 0xB6},
	{0x4643, 0x01},
	{0x4644, 0xFC},
	{0x4646, 0x25},
	{0x4648, 0x82},
	{0x4649, 0x01},
	{0x464A, 0xAB},
	{0x464B, 0x01},
	{0x464C, 0x26},
	{0x464E, 0x30},
	{0x4654, 0xFC},
	{0x4656, 0x08},
	{0x4658, 0x12},
	{0x465A, 0x25},
	{0x4662, 0xFC},
	{0x46A2, 0xFB},
	{0x46D6, 0xF3},
	{0x46E6, 0x00},
	{0x46E8, 0xFF},
	{0x46E9, 0x03},
	{0x46EC, 0x7A},
	{0x46EE, 0xE5},
	{0x46F4, 0xEE},
	{0x46F6, 0xF2},
	{0x470C, 0xFF},
	{0x470D, 0x03},
	{0x470E, 0x00},
	{0x4714, 0xE0},
	{0x4716, 0xE4},
	{0x471E, 0xED},
	{0x472E, 0x00},
	{0x4730, 0xFF},
	{0x4731, 0x03},
	{0x4734, 0x7B},
	{0x4736, 0xDF},
	{0x4754, 0x7D},
	{0x4756, 0x8B},
	{0x4758, 0x93},
	{0x475A, 0xB1},
	{0x475C, 0xFB},
	{0x475E, 0x09},
	{0x4760, 0x11},
	{0x4762, 0x2F},
	{0x4766, 0xCC},
	{0x4776, 0xCB},
	{0x477E, 0x4A},
	{0x478E, 0x49},
	{0x4794, 0x7C},
	{0x4796, 0x8F},
	{0x4798, 0xB3},
	{0x4799, 0x00},
	{0x479A, 0xCC},
	{0x479C, 0xC1},
	{0x479E, 0xCB},
	{0x47A4, 0x7D},
	{0x47A6, 0x8E},
	{0x47A8, 0xB4},
	{0x47A9, 0x00},
	{0x47AA, 0xC0},
	{0x47AC, 0xFA},
	{0x47AE, 0x0D},
	{0x47B0, 0x31},
	{0x47B1, 0x01},
	{0x47B2, 0x4A},
	{0x47B3, 0x01},
	{0x47B4, 0x3F},
	{0x47B6, 0x49},
	{0x47BC, 0xFB},
	{0x47BE, 0x0C},
	{0x47C0, 0x32},
	{0x47C1, 0x01},
	{0x47C2, 0x3E},
	{0x47C3, 0x01},
	{0x4E3C, 0x07},
	{0x3002, 0x00},
	//{0x3000, 0x00},
	{REG_DELAY, 0x1E},//wait_ms(30)
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * 90.059fps
 * CSI-2_2lane
 * AD:10bit Output:12bit
 * 2376Mbps
 * Master Mode
 * Time 9.999ms Gain:6dB
 * 2568x1440 2/2-line binning & Window cropping
 */
static __maybe_unused const struct regval imx678_linear_12bit_1284x720_2376M_regs_2lane[] = {
	{0x3008, 0x5D},
	{0x300A, 0x42},
	{0x301C, 0x04},
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0xAB},
	{0x3025, 0x07},
	{0x3028, 0xA4},
	{0x3029, 0x01},
	{0x3031, 0x00},
	{0x3033, 0x00},
	{0x3040, 0x88},
	{0x3041, 0x02},
	{0x3042, 0x08},
	{0x3043, 0x0A},
	{0x3044, 0xF0},
	{0x3045, 0x02},
	{0x3046, 0x40},
	{0x3047, 0x0B},
	{0x3050, 0xC4},
	{0x3090, 0x14},
	{0x30C1, 0x00},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3116, 0x23},
	{0x3118, 0x08},
	{0x3119, 0x01},
	{0x311A, 0xE7},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},
	{0x4004, 0xC0},
	{0x4005, 0x06},
	{0x4018, 0xE7},
	{0x401A, 0x8F},
	{0x401C, 0x8F},
	{0x401E, 0x7F},
	{0x401F, 0x02},
	{0x4020, 0x97},
	{0x4022, 0x0F},
	{0x4023, 0x01},
	{0x4024, 0x97},
	{0x4026, 0xF7},
	{0x4028, 0x7F},
	{0x3002, 0x00},
	//{0x3000, 0x00},
	{REG_DELAY, 0x1E},//wait_ms(30)
	{REG_NULL, 0x00},
};

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct imx678_mode supported_modes[] = {
	/*
	 * frame rate = 1 / (Vtt * 1H) = 1 / (VMAX * 1H)
	 * VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46
	 */
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x044c * IMX678_4LANES,
		.vts_def = 0x08ca,
		.reg_list = imx678_linear_12bit_3840x2160_1188M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = 0,
		.xvclk = IMX678_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08fc * 2 - 0x0da8,
		.hts_def = 0x0226 * IMX678_4LANES * 2,
		/*
		 * IMX678 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x08fc * 2,
		.reg_list = imx678_hdr2_10bit_3840x2160_1782M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
		.xvclk = IMX678_XVCLK_FREQ_37M,
	},

};

static const s64 link_freq_items[] = {
	MIPI_FREQ_1782M,
	MIPI_FREQ_1188M,
};

/* Write registers up to 4 at a time */
static int imx678_write_reg(struct i2c_client *client, u16 reg,
			    u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int imx678_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	if (!regs) {
		dev_err(&client->dev, "write reg array error\n");
		return ret;
	}
	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr == REG_DELAY) {
			usleep_range(regs[i].val * 1000, regs[i].val * 1000 + 500);
			dev_info(&client->dev, "write reg array, sleep %dms\n", regs[i].val);
		} else {
			ret = imx678_write_reg(client, regs[i].addr,
				IMX678_REG_VALUE_08BIT, regs[i].val);
		}
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int imx678_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			   u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int imx678_get_reso_dist(const struct imx678_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct imx678_mode *
imx678_find_best_fit(struct imx678 *imx678, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx678->cfg_num; i++) {
		dist = imx678_get_reso_dist(&imx678->supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			imx678->supported_modes[i].bus_fmt == framefmt->code) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}
	dev_info(&imx678->client->dev, "%s: cur_best_fit(%d)",
		 __func__, cur_best_fit);

	return &imx678->supported_modes[cur_best_fit];
}

static int __imx678_power_on(struct imx678 *imx678);

static void imx678_change_mode(struct imx678 *imx678, const struct imx678_mode *mode)
{
	if (imx678->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
		imx678->is_thunderboot = false;
		imx678->is_thunderboot_ng = true;
		__imx678_power_on(imx678);
	}
	imx678->cur_mode = mode;
	imx678->cur_vts = imx678->cur_mode->vts_def;
	dev_info(&imx678->client->dev, "set fmt: cur_mode: %dx%d, hdr: %d, bpp: %d\n",
		mode->width, mode->height, mode->hdr_mode, mode->bpp);
}

static int imx678_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode;
	s64 h_blank, vblank_def, vblank_min;
	u64 pixel_rate = 0;
	u8 lanes = imx678->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&imx678->mutex);

	mode = imx678_find_best_fit(imx678, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx678->mutex);
		return -ENOTTY;
#endif
	} else {
		imx678_change_mode(imx678, mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx678->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		/* VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46 */
		vblank_min = (mode->height + 46) - mode->height;
		__v4l2_ctrl_modify_range(imx678->vblank, vblank_min,
					 IMX678_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(imx678->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
			mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(imx678->pixel_rate,
					 pixel_rate);
	}
	dev_info(&imx678->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx678->mutex);

	return 0;
}

static int imx678_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode = imx678->cur_mode;

	mutex_lock(&imx678->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx678->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&imx678->mutex);

	return 0;
}

static int imx678_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (code->index >= imx678->cfg_num)
		return -EINVAL;

	code->code = imx678->supported_modes[code->index].bus_fmt;

	return 0;
}

static int imx678_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (fse->index >= imx678->cfg_num)
		return -EINVAL;

	if (fse->code != imx678->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = imx678->supported_modes[fse->index].width;
	fse->max_width  = imx678->supported_modes[fse->index].width;
	fse->max_height = imx678->supported_modes[fse->index].height;
	fse->min_height = imx678->supported_modes[fse->index].height;

	return 0;
}

static int imx678_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode = imx678->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static int imx678_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct imx678 *imx678 = to_imx678(sd);
	u8 lanes = imx678->bus_cfg.bus.mipi_csi2.num_data_lanes;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = lanes;

	return 0;
}

static void imx678_get_module_inf(struct imx678 *imx678,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX678_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx678->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx678->len_name, sizeof(inf->base.lens));
}

static void imx678_get_pclk_and_tline(struct imx678 *imx678)
{
	const struct imx678_mode *mode = imx678->cur_mode;

	imx678->pclk = (u32)div_u64((u64)mode->hts_def * mode->vts_def *
		mode->max_fps.denominator, mode->max_fps.numerator);
	imx678->tline = (u32)div_u64((u64)mode->hts_def * 1000000000, imx678->pclk);
}

static void imx678_hdr_exposure_readback(struct imx678 *imx678)
{
	u32 shr, shr_l, shr_m, shr_h;
	u32 rhs, rhs_l, rhs_m, rhs_h;
	u32 gain, gain_l, gain_h;
	int ret = 0;

	if (!imx678->is_tline_init) {
		imx678_get_pclk_and_tline(imx678);
		imx678->is_tline_init = true;
	}

	ret = imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_L,
			      IMX678_REG_VALUE_08BIT, &shr_l);
	ret |= imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_M,
			       IMX678_REG_VALUE_08BIT, &shr_m);
	ret |= imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_H,
			       IMX678_REG_VALUE_08BIT, &shr_h);
	if (!ret) {
		shr = (shr_h << 16) | (shr_m << 8) | shr_l;
		imx678->cur_exposure[0] = (imx678->cur_vts - shr) * imx678->tline;
	} else {
		dev_err(&imx678->client->dev,
			"imx678 get exposure of long frame failed!\n");
	}
	ret = imx678_read_reg(imx678->client, IMX678_LF_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, &gain_h);
	ret |= imx678_read_reg(imx678->client, IMX678_LF_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, &gain_l);
	if (!ret) {
		gain = (gain_h << 8) | gain_l;
		imx678->cur_gain[0] = gain * 300;//step=0.3db,factor=1000
	} else {
		dev_err(&imx678->client->dev,
			"imx678 get gain of long frame failed!\n");
	}

	ret = imx678_read_reg(imx678->client, IMX678_SF1_EXPO_REG_L,
			      IMX678_REG_VALUE_08BIT, &shr_l);
	ret |= imx678_read_reg(imx678->client, IMX678_SF1_EXPO_REG_M,
			       IMX678_REG_VALUE_08BIT, &shr_m);
	ret |= imx678_read_reg(imx678->client, IMX678_SF1_EXPO_REG_H,
			       IMX678_REG_VALUE_08BIT, &shr_h);
	ret |= imx678_read_reg(imx678->client, IMX678_RHS1_REG_L,
			      IMX678_REG_VALUE_08BIT, &rhs_l);
	ret |= imx678_read_reg(imx678->client, IMX678_RHS1_REG_M,
			       IMX678_REG_VALUE_08BIT, &rhs_m);
	ret |= imx678_read_reg(imx678->client, IMX678_RHS1_REG_H,
			       IMX678_REG_VALUE_08BIT, &rhs_h);
	if (!ret) {
		shr = (shr_h << 16) | (shr_m << 8) | shr_l;
		rhs = (rhs_h << 16) | (rhs_m << 8) | rhs_l;
		imx678->cur_exposure[1] = (rhs - shr) * imx678->tline;
	} else {
		dev_err(&imx678->client->dev,
			"imx678 get exposure of %s frame failed!\n",
			imx678->cur_mode->hdr_mode == HDR_X2 ?
			"short" : "middle");
	}
	ret = imx678_read_reg(imx678->client, IMX678_SF1_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, &gain_h);
	ret |= imx678_read_reg(imx678->client, IMX678_SF1_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, &gain_l);
	if (!ret) {
		gain = (gain_h << 8) | gain_l;
		imx678->cur_gain[1] = gain * 300;//step=0.3db,factor=1000
	} else {
		dev_err(&imx678->client->dev,
			"imx678 get gain of %s frame failed!\n",
			imx678->cur_mode->hdr_mode == HDR_X2 ?
			"short" : "middle");
	}

	if (imx678->cur_mode->hdr_mode == HDR_X3) {
		ret = imx678_read_reg(imx678->client, IMX678_SF2_EXPO_REG_L,
			      IMX678_REG_VALUE_08BIT, &shr_l);
		ret |= imx678_read_reg(imx678->client, IMX678_SF2_EXPO_REG_M,
				       IMX678_REG_VALUE_08BIT, &shr_m);
		ret |= imx678_read_reg(imx678->client, IMX678_SF2_EXPO_REG_H,
				       IMX678_REG_VALUE_08BIT, &shr_h);
		ret |= imx678_read_reg(imx678->client, IMX678_RHS2_REG_L,
				      IMX678_REG_VALUE_08BIT, &rhs_l);
		ret |= imx678_read_reg(imx678->client, IMX678_RHS2_REG_M,
				       IMX678_REG_VALUE_08BIT, &rhs_m);
		ret |= imx678_read_reg(imx678->client, IMX678_RHS2_REG_H,
				       IMX678_REG_VALUE_08BIT, &rhs_h);
		if (!ret) {
			shr = (shr_h << 16) | (shr_m << 8) | shr_l;
			rhs = (rhs_h << 16) | (rhs_m << 8) | rhs_l;
			imx678->cur_exposure[2] = (rhs - shr) * imx678->tline;
		} else {
			dev_err(&imx678->client->dev,
				"imx678 get exposure of short frame failed!\n");
		}
		ret = imx678_read_reg(imx678->client, IMX678_SF2_GAIN_REG_H,
			IMX678_REG_VALUE_08BIT, &gain_h);
		ret |= imx678_read_reg(imx678->client, IMX678_SF2_GAIN_REG_L,
			IMX678_REG_VALUE_08BIT, &gain_l);
		if (!ret) {
			gain = (gain_h << 8) | gain_l;
			imx678->cur_gain[2] = gain * 300;//step=0.3db,factor=1000
		} else {
			dev_err(&imx678->client->dev,
				"imx678 get gain of short frame failed!\n");
		}
	}
}

static int imx678_set_hdrae_3frame(struct imx678 *imx678,
				   struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx678->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr2, shr1, shr0, rhs2, rhs1 = 0;
	int rhs1_change_limit, rhs2_change_limit = 0;
	int ret = 0;
	u32 fsc;
	int rhs1_max = 0;
	int shr2_min = 0;

	if (!imx678->has_init_exp && !imx678->streaming) {
		imx678->init_hdrae_exp = *ae;
		imx678->has_init_exp = true;
		dev_dbg(&imx678->client->dev, "imx678 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	ret = imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(m_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(m_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF2_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(s_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF2_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 4 * VMAX and FSC should be 6n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -12);
	 *   SHR0 >= RHS2 + 13;
	 *   SHR0 should be 3n;
	 *
	 *   exp_m = RHS1 - SHR1 + Toffset;
	 *
	 *   RHS1 < BRL * 3;
	 *   RHS1 <= SHR2 - 13;
	 *   RHS1 >= SHR1 + 12;
	 *   SHR1 >= 13;
	 *   SHR1 <= RHS1 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR1 should be 3n+1 and RHS1 should be 6n+1;
	 *
	 *   exp_s = RHS2 - SHR2 + Toffset;
	 *
	 *   RHS2 < BRL * 3 + RHS1;
	 *   RHS2 <= SHR0 - 13;
	 *   RHS2 >= SHR2 + 12;
	 *   SHR2 >= RHS1 + 13;
	 *   SHR2 <= RHS2 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR2 should be 3n+2 and RHS2 should be 6n+2;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx678->cur_vts;
	fsc = fsc / 6 * 6;
	shr0 = fsc - l_exp_time;
	dev_dbg(&client->dev,
		"line(%d) shr0 %d, l_exp_time %d, fsc %d\n",
		__LINE__, shr0, l_exp_time, fsc);

	rhs1 = (SHR1_MIN_X3 + m_exp_time + 5) / 6 * 6 + 1;
	if (imx678->cur_mode->height == 2160)
		rhs1_max = RHS1_MAX_X3(BRL_ALL);
	else
		rhs1_max = RHS1_MAX_X3(BRL_BINNING);
	if (rhs1 < 25)
		rhs1 = 25;
	else if (rhs1 > rhs1_max)
		rhs1 = rhs1_max;
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, m_exp_time %d rhs1_old %d\n",
		__LINE__, rhs1, m_exp_time, imx678->rhs1_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx678->cur_mode->height == 2160)
		rhs1_change_limit = imx678->rhs1_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs1_change_limit = imx678->rhs1_old + 3 * BRL_BINNING - fsc + 3;
	rhs1_change_limit = (rhs1_change_limit < 25) ? 25 : rhs1_change_limit;
	rhs1_change_limit = (rhs1_change_limit + 5) / 6 * 6 + 1;
	if (rhs1_max < rhs1_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_change_limit);
		return -EINVAL;
	}
	if (rhs1 < rhs1_change_limit)
		rhs1 = rhs1_change_limit;

	dev_dbg(&client->dev,
		"line(%d) m_exp_time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, m_exp_time, imx678->rhs1_old, rhs1);

	imx678->rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - m_exp_time <= SHR1_MIN_X3) {
		shr1 = SHR1_MIN_X3;
		m_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - m_exp_time;
	}

	shr2_min = rhs1 + 13;
	rhs2 = (shr2_min + s_exp_time + 5) / 6 * 6 + 2;
	if (rhs2 > (shr0 - 13))
		rhs2 = shr0 - 13;
	else if (rhs2 < 50)
		rhs2 = 50;
	dev_dbg(&client->dev,
		"line(%d) rhs2 %d, s_exp_time %d, rhs2_old %d\n",
		__LINE__, rhs2, s_exp_time, imx678->rhs2_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx678->cur_mode->height == 2160)
		rhs2_change_limit = imx678->rhs2_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs2_change_limit = imx678->rhs2_old + 3 * BRL_BINNING - fsc + 3;
	rhs2_change_limit = (rhs2_change_limit < 50) ?  50 : rhs2_change_limit;
	rhs2_change_limit = (rhs2_change_limit + 5) / 6 * 6 + 2;
	if ((shr0 - 13) < rhs2_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs2 max is %d,but old rhs1 limit makes rhs2 min is %d\n",
			shr0 - 13, rhs2_change_limit);
		return -EINVAL;
	}
	if (rhs2 < rhs2_change_limit)
		rhs2 = rhs2_change_limit;

	imx678->rhs2_old = rhs2;

	/* shr2 = rhs2 - s_exp_time */
	if (rhs2 - s_exp_time <= shr2_min) {
		shr2 = shr2_min;
		s_exp_time = rhs2 - shr2;
	} else {
		shr2 = rhs2 - s_exp_time;
	}
	dev_dbg(&client->dev,
		"line(%d) rhs2_new %d, s_exp_time %d shr2 %d, rhs2_change_limit %d\n",
		__LINE__, rhs2, s_exp_time, shr2, rhs2_change_limit);

	if (shr0 < rhs2 + 13)
		shr0 = rhs2 + 13;
	else if (shr0 > fsc - 12)
		shr0 = fsc - 12;

	dev_dbg(&client->dev,
		"long exposure: l_exp_time=%d, fsc=%d, shr0=%d, l_a_gain=%d\n",
		l_exp_time, fsc, shr0, l_a_gain);
	dev_dbg(&client->dev,
		"middle exposure(SEF1): m_exp_time=%d, rhs1=%d, shr1=%d, m_a_gain=%d\n",
		m_exp_time, rhs1, shr1, m_a_gain);
	dev_dbg(&client->dev,
		"short exposure(SEF2): s_exp_time=%d, rhs2=%d, shr2=%d, s_a_gain=%d\n",
		s_exp_time, rhs2, shr2, s_a_gain);
	/* time effect n+1 */
	/* write SEF2 exposure RHS2 regs*/
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_L(rhs2));
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_M(rhs2));
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_H(rhs2));
	/* write SEF2 exposure SHR2 regs*/
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr2));
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr2));
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr2));
	/* write SEF1 exposure RHS1 regs*/
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_L(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_M(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_H(rhs1));
	/* write SEF1 exposure SHR1 regs*/
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr1));
	/* write LF exposure SHR0 regs*/
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr0));

	ret |= imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_END);
	imx678_hdr_exposure_readback(imx678);
	return ret;
}

static int imx678_set_hdrae(struct imx678 *imx678,
			    struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx678->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr1, shr0, rhs1, rhs1_max, rhs1_min;
	int ret = 0;
	u32 fsc;

	if (!imx678->has_init_exp && !imx678->streaming) {
		imx678->init_hdrae_exp = *ae;
		imx678->has_init_exp = true;
		dev_dbg(&imx678->client->dev, "imx678 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (imx678->cur_mode->hdr_mode == HDR_X2) {
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	ret = imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(s_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 2 * VMAX and FSC should be 4n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *   exp_l should be even value;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -8);
	 *   SHR0 >= RHS1 + 9;
	 *   SHR0 should be 2n;
	 *
	 *   exp_s = RHS1 - SHR1 + Toffset;
	 *   exp_s should be even value;
	 *
	 *   RHS1 < BRL * 2;
	 *   RHS1 <= SHR0 - 9;
	 *   RHS1 >= SHR1 + 8;
	 *   SHR1 >= 9;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 2 -FSC + 2;
	 *
	 *   SHR1 should be 2n+1 and RHS1 should be 4n+1;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx678->cur_vts;
	shr0 = fsc - l_exp_time;

	if (imx678->cur_mode->height == 2192) {
		rhs1_max = min(RHS1_MAX_X2(BRL_ALL), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, imx678->rhs1_old + 2 * BRL_ALL - fsc + 2);
	} else {
		rhs1_max = min(RHS1_MAX_X2(BRL_BINNING), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, imx678->rhs1_old + 2 * BRL_BINNING - fsc + 2);
	}
	rhs1_min = (rhs1_min + 3) / 4 * 4 + 1;
	rhs1 = (SHR1_MIN_X2 + s_exp_time + 3) / 4 * 4 + 1;/* shall be 4n + 1 */
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, rhs1 min %d rhs1 max %d\n",
		__LINE__, rhs1, rhs1_min, rhs1_max);
	if (rhs1_max < rhs1_min) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_min);
		return -EINVAL;
	}
	rhs1 = clamp(rhs1, rhs1_min, rhs1_max);
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, short time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, rhs1, s_exp_time, imx678->rhs1_old, rhs1);

	imx678->rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - s_exp_time <= SHR1_MIN_X2) {
		shr1 = SHR1_MIN_X2;
		s_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - s_exp_time;
	}

	if (shr0 < rhs1 + 9)
		shr0 = rhs1 + 9;
	else if (shr0 > fsc - 8)
		shr0 = fsc - 8;

	dev_dbg(&client->dev,
		"fsc=%d,RHS1_MAX=%d,SHR1_MIN=%d,rhs1_max=%d\n",
		fsc, RHS1_MAX_X2(BRL_ALL), SHR1_MIN_X2, rhs1_max);
	dev_dbg(&client->dev,
		"l_exp_time=%d,s_exp_time=%d,shr0=%d,shr1=%d,rhs1=%d,l_a_gain=%d,s_a_gain=%d\n",
		l_exp_time, s_exp_time, shr0, shr1, rhs1, l_a_gain, s_a_gain);
	/* time effect n+2 */
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_L(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_M(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_H(rhs1));

	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr1));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr0));

	ret |= imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_END);
	imx678_hdr_exposure_readback(imx678);
	return ret;
}

static int imx678_get_channel_info(struct imx678 *imx678, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = imx678->cur_mode->vc[ch_info->index];
	ch_info->width = imx678->cur_mode->width;
	ch_info->height = imx678->cur_mode->height;
	ch_info->bus_fmt = imx678->cur_mode->bus_fmt;
	return 0;
}

static long imx678_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	u32 i, h, w, stream;
	long ret = 0;
	const struct imx678_mode *mode;
	u64 pixel_rate = 0;
	struct rkmodule_csi_dphy_param *dphy_param;
	u8 lanes = imx678->bus_cfg.bus.mipi_csi2.num_data_lanes;
	struct rkmodule_exp_delay *exp_delay;
	struct rkmodule_exp_info *exp_info;
	int idx_max = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (imx678->cur_mode->hdr_mode == HDR_X2)
			ret = imx678_set_hdrae(imx678, arg);
		else if (imx678->cur_mode->hdr_mode == HDR_X3)
			ret = imx678_set_hdrae_3frame(imx678, arg);
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx678_get_module_inf(imx678, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx678->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx678->cur_mode->width;
		h = imx678->cur_mode->height;
		for (i = 0; i < imx678->cfg_num; i++) {
			if (w == imx678->supported_modes[i].width &&
			    h == imx678->supported_modes[i].height &&
			    imx678->supported_modes[i].hdr_mode == hdr->hdr_mode) {
				dev_info(&imx678->client->dev, "set hdr cfg, set mode to %d\n", i);
				imx678_change_mode(imx678, &imx678->supported_modes[i]);
				break;
			}
		}
		if (i == imx678->cfg_num) {
			dev_err(&imx678->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			mode = imx678->cur_mode;
			if (imx678->streaming) {
				ret = imx678_write_reg(imx678->client, IMX678_GROUP_HOLD_REG,
					IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_START);

				ret |= imx678_write_array(imx678->client,
							  imx678->cur_mode->reg_list);

				ret |= imx678_write_reg(imx678->client, IMX678_GROUP_HOLD_REG,
					IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_END);
				if (ret)
					return ret;
			}
			w = mode->hts_def - imx678->cur_mode->width;
			h = mode->vts_def - mode->height;
			mutex_lock(&imx678->mutex);
			__v4l2_ctrl_modify_range(imx678->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx678->vblank, h,
				IMX678_VTS_MAX - mode->height,
				1, h);
			__v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx);
			pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
				mode->bpp * 2 * lanes;
			__v4l2_ctrl_s_ctrl_int64(imx678->pixel_rate,
						 pixel_rate);
			mutex_unlock(&imx678->mutex);
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_STREAMING);
		else
			ret = imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_SONY_BRL:
		if (imx678->cur_mode->width == 3840 && imx678->cur_mode->height == 2160)
			*((u32 *)arg) = BRL_ALL;
		else
			*((u32 *)arg) = BRL_BINNING;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx678_get_channel_info(imx678, ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		if (imx678->cur_mode->hdr_mode == HDR_X2) {
			dphy_param = (struct rkmodule_csi_dphy_param *)arg;
			*dphy_param = dcphy_param;
			dev_info(&imx678->client->dev,
				 "get sensor dphy param\n");
		} else
			ret = -EINVAL;
		break;
	case RKMODULE_GET_EXP_DELAY:
		exp_delay = (struct rkmodule_exp_delay *)arg;
		exp_delay->exp_delay = 2;
		exp_delay->gain_delay = 2;
		exp_delay->vts_delay = 1;
		break;
	case RKMODULE_GET_EXP_INFO:
		exp_info = (struct rkmodule_exp_info *)arg;
		if (imx678->cur_mode->hdr_mode == NO_HDR)
			idx_max = 1;
		else if (imx678->cur_mode->hdr_mode == HDR_X2)
			idx_max = 2;
		else
			idx_max = 3;
		for (i = 0; i < idx_max; i++) {
			exp_info->exp[i] = imx678->cur_exposure[i];
			exp_info->gain[i] = imx678->cur_gain[i];
		}
		exp_info->hts = imx678->cur_mode->hts_def;
		exp_info->vts = imx678->cur_vts;
		exp_info->pclk = imx678->pclk;
		exp_info->gain_mode.gain_mode = RKMODULE_GAIN_MODE_DB;
		exp_info->gain_mode.factor = 1000;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx678_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32  stream;
	u32 brl = 0;
	struct rkmodule_csi_dphy_param *dphy_param;
	struct rkmodule_exp_delay *exp_delay;
	struct rkmodule_exp_info *exp_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf))) {
				kfree(inf);
				return -EFAULT;
			}
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(cfg, up, sizeof(*cfg))) {
			kfree(cfg);
			return -EFAULT;
		}
		ret = imx678_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr))) {
				kfree(hdr);
				return -EFAULT;
			}
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdr, up, sizeof(*hdr))) {
			kfree(hdr);
			return -EFAULT;
		}
		ret = imx678_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdrae, up, sizeof(*hdrae))) {
			kfree(hdrae);
			return -EFAULT;
		}
		ret = imx678_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;
		ret = imx678_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = imx678_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;
	case RKMODULE_GET_EXP_DELAY:
		exp_delay = kzalloc(sizeof(*exp_delay), GFP_KERNEL);
		if (!exp_delay) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, exp_delay);
		if (!ret) {
			ret = copy_to_user(up, exp_delay, sizeof(*exp_delay));
			if (ret)
				ret = -EFAULT;
		}
		kfree(exp_delay);
		break;
	case RKMODULE_GET_EXP_INFO:
		exp_info = kzalloc(sizeof(*exp_info), GFP_KERNEL);
		if (!exp_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, exp_info);
		if (!ret) {
			ret = copy_to_user(up, exp_info, sizeof(*exp_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(exp_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __imx678_start_stream(struct imx678 *imx678)
{
	int ret;

	if (!imx678->is_thunderboot) {
		ret = imx678_write_array(imx678->client, imx678->cur_mode->reg_list);

		if (ret)
			return ret;
	}
	imx678_get_pclk_and_tline(imx678);

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx678->ctrl_handler);
	if (ret)
		return ret;
	if (imx678->has_init_exp && imx678->cur_mode->hdr_mode != NO_HDR) {
		imx678->rhs1_old = IMX678_RHS1_DEFAULT;
		imx678->rhs2_old = IMX678_RHS2_DEFAULT;
		ret = imx678_ioctl(&imx678->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx678->init_hdrae_exp);
		if (ret) {
			dev_err(&imx678->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}
	return imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, 0);
}

static int __imx678_stop_stream(struct imx678 *imx678)
{
	imx678->has_init_exp = false;
	if (imx678->is_thunderboot)
		imx678->is_first_streamoff = true;
	imx678->is_tline_init = false;
	return imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, 1);
}

static int imx678_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct i2c_client *client = imx678->client;
	int ret = 0;

	dev_info(&imx678->client->dev, "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n",
	       on, imx678->cur_mode->width, imx678->cur_mode->height,
	       imx678->cur_mode->hdr_mode, imx678->cur_mode->bpp);

	mutex_lock(&imx678->mutex);
	on = !!on;
	if (on == imx678->streaming)
		goto unlock_and_return;

	if (on) {
		if (imx678->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			imx678->is_thunderboot = false;
			__imx678_power_on(imx678);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx678_start_stream(imx678);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx678_stop_stream(imx678);
		pm_runtime_put(&client->dev);
	}

	imx678->streaming = on;

unlock_and_return:
	mutex_unlock(&imx678->mutex);

	return ret;
}

static int imx678_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct i2c_client *client = imx678->client;
	int ret = 0;

	mutex_lock(&imx678->mutex);

	if (imx678->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		imx678->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx678->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx678->mutex);

	return ret;
}

int __imx678_power_on(struct imx678 *imx678)
{
	int ret;
	struct device *dev = &imx678->client->dev;

	if (!IS_ERR_OR_NULL(imx678->pins_default)) {
		ret = pinctrl_select_state(imx678->pinctrl,
					   imx678->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	if (!imx678->is_thunderboot) {
		if (!IS_ERR(imx678->power_gpio))
			gpiod_direction_output(imx678->power_gpio, 1);
		/* At least 500ns between power raising and XCLR */
		/* fix power on timing if insmod this ko */
		usleep_range(10 * 1000, 20 * 1000);
		if (!IS_ERR(imx678->reset_gpio))
			gpiod_direction_output(imx678->reset_gpio, 0);

		/* At least 1us between XCLR and clk */
		/* fix power on timing if insmod this ko */
		usleep_range(10 * 1000, 20 * 1000);
	}
	ret = clk_set_rate(imx678->xvclk, imx678->cur_mode->xvclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(imx678->xvclk) != imx678->cur_mode->xvclk)
		dev_warn(dev, "xvclk mismatched\n");
	ret = clk_prepare_enable(imx678->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	cam_sw_regulator_bulk_init(imx678->cam_sw_inf, IMX678_NUM_SUPPLIES, imx678->supplies);

	if (imx678->is_thunderboot)
		return 0;

	ret = regulator_bulk_enable(IMX678_NUM_SUPPLIES, imx678->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto err_pinctrl;
	}

	/* At least 20us between XCLR and I2C communication */
	usleep_range(20*1000, 30*1000);

	return 0;

err_pinctrl:
	clk_disable_unprepare(imx678->xvclk);

err_clk:
	if (!IS_ERR(imx678->reset_gpio))
		gpiod_direction_output(imx678->reset_gpio, 1);
	if (!IS_ERR_OR_NULL(imx678->pins_sleep))
		pinctrl_select_state(imx678->pinctrl, imx678->pins_sleep);

	return ret;
}

static void __imx678_power_off(struct imx678 *imx678)
{
	int ret;
	struct device *dev = &imx678->client->dev;

	if (imx678->is_thunderboot) {
		if (imx678->is_first_streamoff) {
			imx678->is_thunderboot = false;
			imx678->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(imx678->reset_gpio))
		gpiod_direction_output(imx678->reset_gpio, 1);
	clk_disable_unprepare(imx678->xvclk);
	if (!IS_ERR_OR_NULL(imx678->pins_sleep)) {
		ret = pinctrl_select_state(imx678->pinctrl,
					   imx678->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(imx678->power_gpio))
		gpiod_direction_output(imx678->power_gpio, 0);
	regulator_bulk_disable(IMX678_NUM_SUPPLIES, imx678->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused imx678_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	cam_sw_prepare_wakeup(imx678->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(imx678->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&imx678->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (imx678->has_init_exp && imx678->cur_mode != NO_HDR) {	// hdr mode
		ret = imx678_ioctl(&imx678->subdev, PREISP_CMD_SET_HDRAE_EXP,
				    &imx678->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&imx678->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int __maybe_unused imx678_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	cam_sw_write_array_cb_init(imx678->cam_sw_inf, client,
				   (void *)imx678->cur_mode->reg_list,
				   (sensor_write_array)imx678_write_array);
	cam_sw_prepare_sleep(imx678->cam_sw_inf);

	return 0;
}
#else
#define imx678_resume NULL
#define imx678_suspend NULL
#endif

static int __maybe_unused imx678_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	return __imx678_power_on(imx678);
}

static int __maybe_unused imx678_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	__imx678_power_off(imx678);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx678_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx678_mode *def_mode = &imx678->supported_modes[0];

	mutex_lock(&imx678->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx678->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx678_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (fie->index >= imx678->cfg_num)
		return -EINVAL;

	fie->code = imx678->supported_modes[fie->index].bus_fmt;
	fie->width = imx678->supported_modes[fie->index].width;
	fie->height = imx678->supported_modes[fie->index].height;
	fie->interval = imx678->supported_modes[fie->index].max_fps;
	fie->reserved[0] = imx678->supported_modes[fie->index].hdr_mode;
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160
#define DST_WIDTH_1920 1920
#define DST_HEIGHT_1080 1080

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int imx678_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (imx678->cur_mode->width == 3840) {
			sel->r.left = CROP_START(imx678->cur_mode->width, DST_WIDTH_3840);
			sel->r.width = DST_WIDTH_3840;
			sel->r.top = CROP_START(imx678->cur_mode->height, DST_HEIGHT_2160);
			sel->r.height = DST_HEIGHT_2160;
		} else if (imx678->cur_mode->width == 1944) {
			sel->r.left = CROP_START(imx678->cur_mode->width, DST_WIDTH_1920);
			sel->r.width = DST_WIDTH_1920;
			sel->r.top = CROP_START(imx678->cur_mode->height, DST_HEIGHT_1080);
			sel->r.height = DST_HEIGHT_1080;
		} else {
			sel->r.left = CROP_START(imx678->cur_mode->width, imx678->cur_mode->width);
			sel->r.width = imx678->cur_mode->width;
			sel->r.top = CROP_START(imx678->cur_mode->height, imx678->cur_mode->height);
			sel->r.height = imx678->cur_mode->height;
		}
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops imx678_pm_ops = {
	SET_RUNTIME_PM_OPS(imx678_runtime_suspend,
			   imx678_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(imx678_suspend, imx678_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx678_internal_ops = {
	.open = imx678_open,
};
#endif

static const struct v4l2_subdev_core_ops imx678_core_ops = {
	.s_power = imx678_s_power,
	.ioctl = imx678_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx678_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx678_video_ops = {
	.s_stream = imx678_s_stream,
	.g_frame_interval = imx678_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx678_pad_ops = {
	.enum_mbus_code = imx678_enum_mbus_code,
	.enum_frame_size = imx678_enum_frame_sizes,
	.enum_frame_interval = imx678_enum_frame_interval,
	.get_fmt = imx678_get_fmt,
	.set_fmt = imx678_set_fmt,
	.get_selection = imx678_get_selection,
	.get_mbus_config = imx678_g_mbus_config,
};

static const struct v4l2_subdev_ops imx678_subdev_ops = {
	.core	= &imx678_core_ops,
	.video	= &imx678_video_ops,
	.pad	= &imx678_pad_ops,
};

static void imx678_exposure_readback(struct imx678 *imx678)
{
	u32 shr, shr_l, shr_m, shr_h;
	int ret = 0;

	if (!imx678->is_tline_init) {
		imx678_get_pclk_and_tline(imx678);
		imx678->is_tline_init = true;
	}

	ret = imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_L,
			      IMX678_REG_VALUE_08BIT, &shr_l);
	ret |= imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_M,
			       IMX678_REG_VALUE_08BIT, &shr_m);
	ret |= imx678_read_reg(imx678->client, IMX678_LF_EXPO_REG_H,
			       IMX678_REG_VALUE_08BIT, &shr_h);
	if (!ret) {
		shr = (shr_h << 16) | (shr_m << 8) | shr_l;
		imx678->cur_exposure[0] = (imx678->cur_vts - shr) * imx678->tline;
	}
}

static int imx678_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx678 *imx678 = container_of(ctrl->handler,
					     struct imx678, ctrl_handler);
	struct i2c_client *client = imx678->client;
	s64 max;
	u32 vts = 0, val;
	int ret = 0;
	u32 shr0 = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		if (imx678->cur_mode->hdr_mode == NO_HDR) {
			/* Update max exposure while meeting expected vblanking */
			max = imx678->cur_mode->height + ctrl->val - 8;
			__v4l2_ctrl_modify_range(imx678->exposure,
					 imx678->exposure->minimum, max,
					 imx678->exposure->step,
					 imx678->exposure->default_value);
		}
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (imx678->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		shr0 = imx678->cur_vts - ctrl->val;
		ret = imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_L,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_EXP_L(shr0));
		ret |= imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_M,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_EXP_M(shr0));
		ret |= imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_H,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_EXP_H(shr0));
		imx678_exposure_readback(imx678);
		dev_dbg(&client->dev, "set exposure(shr0) %d = cur_vts(%d) - val(%d)\n",
			shr0, imx678->cur_vts, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (imx678->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		ret = imx678_write_reg(imx678->client, IMX678_LF_GAIN_REG_H,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_GAIN_H(ctrl->val));
		ret |= imx678_write_reg(imx678->client, IMX678_LF_GAIN_REG_L,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_GAIN_L(ctrl->val));
		dev_dbg(&client->dev, "set analog gain 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx678->cur_mode->height;
		/*
		 * vts of hdr mode is double to correct T-line calculation.
		 * Restore before write to reg.
		 */
		if (imx678->cur_mode->hdr_mode == HDR_X2) {
			vts = (vts + 3) / 4 * 4;
			imx678->cur_vts = vts;
			vts /= 2;
		} else if (imx678->cur_mode->hdr_mode == HDR_X3) {
			vts = (vts + 11) / 12 * 12;
			imx678->cur_vts = vts;
			vts /= 4;
		} else {
			imx678->cur_vts = vts;
		}
		ret = imx678_write_reg(imx678->client, IMX678_VTS_REG_L,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_L(vts));
		ret |= imx678_write_reg(imx678->client, IMX678_VTS_REG_M,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_M(vts));
		ret |= imx678_write_reg(imx678->client, IMX678_VTS_REG_H,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_H(vts));
		dev_dbg(&client->dev, "set vblank 0x%x vts %d\n",
			ctrl->val, vts);
		break;
	case V4L2_CID_HFLIP:
		ret = imx678_read_reg(imx678->client, IMX678_MIRROR_REG,
				      IMX678_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX678_MIRROR_BIT_MASK;
		else
			val &= ~IMX678_MIRROR_BIT_MASK;
		ret = imx678_write_reg(imx678->client, IMX678_MIRROR_REG,
				       IMX678_REG_VALUE_08BIT, val);
		break;
	case V4L2_CID_VFLIP:
		ret = imx678_read_reg(imx678->client, IMX678_FLIP_REG,
				      IMX678_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX678_FLIP_BIT_MASK;
		else
			val &= ~IMX678_FLIP_BIT_MASK;
		ret = imx678_write_reg(imx678->client, IMX678_FLIP_REG,
				       IMX678_REG_VALUE_08BIT, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

ctrl_end:
	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx678_ctrl_ops = {
	.s_ctrl = imx678_set_ctrl,
};

static int imx678_initialize_controls(struct imx678 *imx678)
{
	const struct imx678_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 pixel_rate;
	u64 max_pixel_rate;
	u32 h_blank;
	int ret;
	u8 lanes = imx678->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &imx678->ctrl_handler;
	mode = imx678->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx678->mutex;

	imx678->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * lanes;
	max_pixel_rate = MIPI_FREQ_1188M / mode->bpp * 2 * lanes;
	imx678->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, max_pixel_rate,
		1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	imx678->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (imx678->hblank)
		imx678->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx678->vblank = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				IMX678_VTS_MAX - mode->height,
				1, vblank_def);
	imx678->cur_vts = mode->vts_def;

	exposure_max = mode->vts_def - 8;
	imx678->exposure = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_EXPOSURE, IMX678_EXPOSURE_MIN,
				exposure_max, IMX678_EXPOSURE_STEP,
				mode->exp_def);
	imx678->anal_a_gain = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, IMX678_GAIN_MIN,
				IMX678_GAIN_MAX, IMX678_GAIN_STEP,
				IMX678_GAIN_DEFAULT);
	v4l2_ctrl_new_std(handler, &imx678_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &imx678_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx678->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx678->subdev.ctrl_handler = handler;
	imx678->has_init_exp = false;
	imx678->is_tline_init = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx678_check_sensor_id(struct imx678 *imx678,
				  struct i2c_client *client)
{
	struct device *dev = &imx678->client->dev;
	u32 id = 0;
	int ret;

	if (imx678->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = imx678_read_reg(client, IMX678_REG_CHIP_ID,
			      IMX678_REG_VALUE_08BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected imx678 id %06x\n", CHIP_ID);

	return 0;
}

static int imx678_configure_regulators(struct imx678 *imx678)
{
	unsigned int i;

	for (i = 0; i < IMX678_NUM_SUPPLIES; i++)
		imx678->supplies[i].supply = imx678_supply_names[i];

	return devm_regulator_bulk_get(&imx678->client->dev,
				       IMX678_NUM_SUPPLIES,
				       imx678->supplies);
}

static int imx678_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx678 *imx678;
	struct v4l2_subdev *sd;
	struct device_node *endpoint;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	imx678 = devm_kzalloc(dev, sizeof(*imx678), GFP_KERNEL);
	if (!imx678)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx678->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx678->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx678->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx678->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&imx678->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	imx678->client = client;
	if (imx678->bus_cfg.bus.mipi_csi2.num_data_lanes == IMX678_4LANES) {
		imx678->supported_modes = supported_modes;
		imx678->cfg_num = ARRAY_SIZE(supported_modes);
	} else {
		imx678->supported_modes = supported_modes;
		imx678->cfg_num = ARRAY_SIZE(supported_modes);
	}
	dev_info(dev, "detect imx678 lane %d\n",
		imx678->bus_cfg.bus.mipi_csi2.num_data_lanes);

	for (i = 0; i < imx678->cfg_num; i++) {
		if (hdr_mode == imx678->supported_modes[i].hdr_mode) {
			imx678->cur_mode = &imx678->supported_modes[i];
			break;
		}
	}

	of_property_read_u32(node, RKMODULE_CAMERA_FASTBOOT_ENABLE,
		&imx678->is_thunderboot);

	imx678->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx678->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx678->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(imx678->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");
	imx678->power_gpio = devm_gpiod_get(dev, "power", GPIOD_ASIS);
	if (IS_ERR(imx678->power_gpio))
		dev_warn(dev, "Failed to get power-gpios\n");
	imx678->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx678->pinctrl)) {
		imx678->pins_default =
			pinctrl_lookup_state(imx678->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx678->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		imx678->pins_sleep =
			pinctrl_lookup_state(imx678->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx678->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	ret = imx678_configure_regulators(imx678);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx678->mutex);

	sd = &imx678->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx678_subdev_ops);
	ret = imx678_initialize_controls(imx678);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx678_power_on(imx678);
	if (ret)
		goto err_free_handler;

	ret = imx678_check_sensor_id(imx678, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx678_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx678->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx678->pad);
	if (ret < 0)
		goto err_power_off;
#endif
	memset(facing, 0, sizeof(facing));
	if (strcmp(imx678->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx678->module_index, facing,
		 IMX678_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__imx678_power_off(imx678);
err_free_handler:
	v4l2_ctrl_handler_free(&imx678->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx678->mutex);

	return ret;
}

static void imx678_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx678->ctrl_handler);
	mutex_destroy(&imx678->mutex);

	cam_sw_deinit(imx678->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx678_power_off(imx678);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx678_of_match[] = {
	{ .compatible = "sony,imx678" },
	{},
};
MODULE_DEVICE_TABLE(of, imx678_of_match);
#endif

static const struct i2c_device_id imx678_match_id[] = {
	{ "sony,imx678", 0 },
	{ },
};

static struct i2c_driver imx678_i2c_driver = {
	.driver = {
		.name = IMX678_NAME,
		.pm = &imx678_pm_ops,
		.of_match_table = of_match_ptr(imx678_of_match),
	},
	.probe		= &imx678_probe,
	.remove		= &imx678_remove,
	.id_table	= imx678_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx678_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx678_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx678 sensor driver");
MODULE_LICENSE("GPL");
