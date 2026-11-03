// SPDX-License-Identifier: GPL-2.0
/*
 * gc02m0 driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 */

/* Register settings follow the stock Torder R10D sensor configuration. */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>

#include <linux/regulator/consumer.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x02)

#define GC02M0_MIPI_LINK_FREQ	328000000

/* Pixel rate = link frequency * 2 * lanes / bits per sample. */
#define GC02M0_PIXEL_RATE		(GC02M0_MIPI_LINK_FREQ * 2LL * 1LL / 10)
#define GC02M0_XVCLK_FREQ		24000000

#define CHIP_ID					0x02e0
#define GC02M0_REG_CHIP_ID_H	0xf0
#define GC02M0_REG_CHIP_ID_L	0xf1
#define SENSOR_ID(_msb, _lsb)	((_msb) << 8 | (_lsb))

#define GC02M0_PAGE_SELECT		0xfe
#define GC02M0_MODE_SELECT		0x3e
#define GC02M0_MODE_SW_STANDBY	0x00
#define GC02M0_MODE_STREAMING	0x90

#define GC02M0_REG_EXPOSURE_H	0x03
#define GC02M0_REG_EXPOSURE_L	0x04
#define	GC02M0_EXPOSURE_MIN		4
#define	GC02M0_EXPOSURE_STEP	1
#define GC02M0_VTS_MAX			0x3fff

#define GC02M0_ANALOG_GAIN_REG	0xb6
#define GC02M0_PREGAIN_H_REG	0xb1
#define GC02M0_PREGAIN_L_REG	0xb2
#define GC02M0_GAIN_MIN			0x400
#define GC02M0_GAIN_MAX			0x3000
#define GC02M0_GAIN_STEP		1
#define GC02M0_GAIN_DEFAULT		0x400

#define GC02M0_REG_VTS_H		0x41
#define GC02M0_REG_VTS_L		0x42

#define GC02M0_LANES			1
#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define GC02M0_NAME			"gc02m0"
#define REG_NULL				0xFF

static const char * const gc02m0_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
	"dvdd",		/* Digital core power */
};

#define GC02M0_NUM_SUPPLIES ARRAY_SIZE(gc02m0_supply_names)

#define to_gc02m0(sd) container_of(sd, struct gc02m0, subdev)

struct regval {
	u8 addr;
	u8 val;
};

struct gc02m0_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct gc02m0_reset {
	struct list_head list;
	struct gpio_desc *gpio;
	unsigned int consumers;
	unsigned int active_users;
};

static LIST_HEAD(gc02m0_reset_lines);
static DEFINE_MUTEX(gc02m0_reset_mutex);

struct gc02m0 {
	struct i2c_client	*client;
	const char		*sensor_name;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gc02m0_reset	*reset;
	bool			reset_active;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[GC02M0_NUM_SUPPLIES];
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;
	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	/* Protects streaming state and controls. */
	struct mutex		mutex;
	bool			streaming;
	bool			stream_pm_held;
	/* Tracks the s_power reference, not runtime power state. */
	bool			power_on;
	const struct gc02m0_mode *cur_mode;
	u32			cur_vts;
	unsigned int	lane_num;
	unsigned int	pixel_rate;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
};

/*
 * Xclk 24Mhz
 */
static const struct regval gc02m0_global_regs[] = {
	{0xfc, 0x01},
	{0xf4, 0x41},
	{0xf5, 0xc0},
	{0xf6, 0x44},
	{0xf8, 0x38},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfd, 0x80},
	{0xfc, 0x81},
	{0xfe, 0x03},
	{0x01, 0x0b},
	{0xf7, 0x01},
	{0xfc, 0x80},
	{0xfc, 0x80},
	{0xfc, 0x80},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0x87, 0x09},
	{0xee, 0x72},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},
	{0x90, 0x00},
	{0x03, 0x04},
	{0x04, 0x7d},
	{0x41, 0x04},
	{0x42, 0xf4},
	{0x05, 0x04},
	{0x06, 0x48},
	{0x07, 0x00},
	{0x08, 0x18},
	{0x9d, 0x18},
	{0x09, 0x00},
	{0x0a, 0x02},
	{0x0d, 0x04},
	{0x0e, 0xbc},
	{0x17, 0x80},
	{0x19, 0x04},
	{0x24, 0x00},
	{0x56, 0x20},
	{0x5b, 0x00},
	{0x5e, 0x01},
	{0x21, 0x3c},
	{0x44, 0x20},
	{0xcc, 0x01},
	{0x1a, 0x04},
	{0x1f, 0x11},
	{0x27, 0x30},
	{0x2b, 0x00},
	{0x33, 0x00},
	{0x53, 0x90},
	{0xe6, 0x50},
	{0x39, 0x07},
	{0x43, 0x04},
	{0x46, 0x2a},
	{0x7c, 0xa0},
	{0xd0, 0xbe},
	{0xd1, 0x60},
	{0xd2, 0x40},
	{0xd3, 0xf3},
	{0xde, 0x1d},
	{0xcd, 0x05},
	{0xce, 0x6f},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x04},
	{0xe0, 0x01},
	{0xfe, 0x00},
	{0xfe, 0x01},
	{0x53, 0x44},
	{0x87, 0x53},
	{0x89, 0x03},
	{0xfe, 0x00},
	{0xb0, 0x74},
	{0xb1, 0x04},
	{0xb2, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x04},
	{0xd8, 0x00},
	{0xc0, 0x40},
	{0xc0, 0x00},
	{0xc0, 0x00},
	{0xc0, 0x00},
	{0xc0, 0x60},
	{0xc0, 0x00},
	{0xc0, 0xc0},
	{0xc0, 0x2a},
	{0xc0, 0x80},
	{0xc0, 0x00},
	{0xc0, 0x00},
	{0xc0, 0x40},
	{0xc0, 0xa0},
	{0xc0, 0x00},
	{0xc0, 0x90},
	{0xc0, 0x19},
	{0xc0, 0xc0},
	{0xc0, 0x00},
	{0xc0, 0xd0},
	{0xc0, 0x2f},
	{0xc0, 0xe0},
	{0xc0, 0x00},
	{0xc0, 0x90},
	{0xc0, 0x39},
	{0xc0, 0x00},
	{0xc0, 0x01},
	{0xc0, 0x20},
	{0xc0, 0x04},
	{0xc0, 0x20},
	{0xc0, 0x01},
	{0xc0, 0xe0},
	{0xc0, 0x0f},
	{0xc0, 0x40},
	{0xc0, 0x01},
	{0xc0, 0xe0},
	{0xc0, 0x1a},
	{0xc0, 0x60},
	{0xc0, 0x01},
	{0xc0, 0x20},
	{0xc0, 0x25},
	{0xc0, 0x80},
	{0xc0, 0x01},
	{0xc0, 0xa0},
	{0xc0, 0x2c},
	{0xc0, 0xa0},
	{0xc0, 0x01},
	{0xc0, 0xe0},
	{0xc0, 0x32},
	{0xc0, 0xc0},
	{0xc0, 0x01},
	{0xc0, 0x20},
	{0xc0, 0x38},
	{0xc0, 0xe0},
	{0xc0, 0x01},
	{0xc0, 0x60},
	{0xc0, 0x3c},
	{0xc0, 0x00},
	{0xc0, 0x02},
	{0xc0, 0xa0},
	{0xc0, 0x40},
	{0xc0, 0x80},
	{0xc0, 0x02},
	{0xc0, 0x18},
	{0xc0, 0x5c},
	{0xfe, 0x00},
	{0x9f, 0x10},
	{0xfe, 0x00},
	{0x26, 0x20},
	{0xfe, 0x01},
	{0x40, 0x22},
	{0x46, 0x7f},
	{0x49, 0x0f},
	{0x4a, 0xf0},
	{0xfe, 0x04},
	{0x14, 0x80},
	{0x15, 0x80},
	{0x16, 0x80},
	{0x17, 0x80},
	{0xfe, 0x01},
	{0x41, 0x20},
	{0x4c, 0x00},
	{0x4d, 0x0c},
	{0x44, 0x08},
	{0x48, 0x03},
	{0xfe, 0x01},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x06},
	{0x93, 0x00},
	{0x94, 0x06},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	{0xfe, 0x03},
	{0x01, 0x23},
	{0x03, 0xce},
	{0x04, 0x48},
	{0x15, 0x02},
	{0x21, 0x10},
	{0x22, 0x05},
	{0x23, 0x20},
	{0x25, 0x20},
	{0x26, 0x08},
	{0x29, 0x06},
	{0x2a, 0x0a},
	{0x2b, 0x08},
	{0xfe, 0x01},
	{0x8c, 0x10},
	{0xfe, 0x00},
	{0xff, 0x00},
};

static const struct gc02m0_mode supported_modes[] = {
	{
		.width = 1600,
		.height = 1200,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.exp_def = 0x0480,
		.hts_def = 0x0448 * 2,
		.vts_def = 0x04fc,
		.reg_list = gc02m0_global_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	GC02M0_MIPI_LINK_FREQ
};

static int gc02m0_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xFF;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret == 1)
		return 0;
	if (ret >= 0)
		ret = -EIO;

	dev_err(&client->dev,
		"gc02m0 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int gc02m0_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gc02m0_write_reg(client, regs[i].addr, regs[i].val);
	return ret;
}

static int gc02m0_read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret == 2) {
		*val = buf[0];
		return 0;
	}

	if (ret >= 0)
		ret = -EIO;
	dev_err(&client->dev,
		"gc02m0 read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc02m0_get_reso_dist(const struct gc02m0_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct gc02m0_mode *
gc02m0_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = gc02m0_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static const u32 gc02m0_agc_params[16][2] = {
	{1024, 0},
	{1536, 1},
	{2035, 2},
	{2519, 3},
	{3165, 4},
	{3626, 5},
	{4147, 6},
	{4593, 7},
	{5095, 8},
	{5697, 9},
	{6270, 10},
	{6714, 11},
	{7210, 12},
	{7686, 13},
	{8214, 14},
	{10337, 15},
};

static int gc02m0_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	const struct gc02m0_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc02m0->mutex);

	mode = gc02m0_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc02m0->mutex);
		return -ENOTTY;
#endif
	} else {
		gc02m0->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc02m0->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc02m0->vblank, vblank_def,
					 GC02M0_VTS_MAX - mode->height,
					 1, vblank_def);
	}

	mutex_unlock(&gc02m0->mutex);

	return 0;
}

static int gc02m0_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	const struct gc02m0_mode *mode;

	mutex_lock(&gc02m0->mutex);
	mode = gc02m0->cur_mode;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc02m0->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		/* format info: width/height/data type/virctual channel */
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&gc02m0->mutex);

	return 0;
}

static int gc02m0_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);

	if (code->index != 0)
		return -EINVAL;

	code->code = gc02m0->cur_mode->bus_fmt;

	return 0;
}

static int gc02m0_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gc02m0_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	const struct gc02m0_mode *mode;
	u32 vts;

	mutex_lock(&gc02m0->mutex);
	mode = gc02m0->cur_mode;
	vts = gc02m0->cur_vts;
	fi->interval = mode->max_fps;
	if (vts != mode->vts_def) {
		fi->interval.numerator *= vts;
		fi->interval.denominator *= mode->vts_def;
	}
	mutex_unlock(&gc02m0->mutex);

	return 0;
}

static void gc02m0_get_module_inf(struct gc02m0 *gc02m0,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, gc02m0->sensor_name,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module, gc02m0->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gc02m0->len_name, sizeof(inf->base.lens));
}

static long gc02m0_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	struct rkmodule_hdr_cfg *hdr_cfg;
	long ret = 0;
	int i, w, h;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc02m0_get_module_inf(gc02m0, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		mutex_lock(&gc02m0->mutex);
		stream = *((u32 *)arg);
		if (!gc02m0->stream_pm_held)
			ret = -EPIPE;
		else if (stream) {
			ret = gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
			ret |= gc02m0_write_reg(gc02m0->client, GC02M0_MODE_SELECT,
						 GC02M0_MODE_STREAMING);
			ret |= gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
		} else {
			ret = gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
			ret |= gc02m0_write_reg(gc02m0->client, GC02M0_MODE_SELECT,
						 GC02M0_MODE_SW_STANDBY);
			ret |= gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
		}
		mutex_unlock(&gc02m0->mutex);
		break;

	case RKMODULE_GET_HDR_CFG:
		mutex_lock(&gc02m0->mutex);
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		hdr_cfg->esp.mode = HDR_NORMAL_VC;
		hdr_cfg->hdr_mode = gc02m0->cur_mode->hdr_mode;
		mutex_unlock(&gc02m0->mutex);
		break;
	case RKMODULE_SET_HDR_CFG:
		mutex_lock(&gc02m0->mutex);
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		w = gc02m0->cur_mode->width;
		h = gc02m0->cur_mode->height;
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr_cfg->hdr_mode) {
				gc02m0->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(supported_modes)) {
			dev_err(&gc02m0->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = gc02m0->cur_mode->hts_def - gc02m0->cur_mode->width;
			h = gc02m0->cur_mode->vts_def - gc02m0->cur_mode->height;
			__v4l2_ctrl_modify_range(gc02m0->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(gc02m0->vblank, h,
						 GC02M0_VTS_MAX - gc02m0->cur_mode->height, 1, h);
		}
		mutex_unlock(&gc02m0->mutex);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc02m0_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc02m0_ioctl(sd, cmd, inf);
		if (!ret)
			ret = copy_to_user(up, inf, sizeof(*inf)) ? -EFAULT : 0;
		kfree(inf);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc02m0_ioctl(sd, cmd, hdr);
		if (!ret)
			ret = copy_to_user(up, hdr, sizeof(*hdr)) ? -EFAULT : 0;
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdr, up, sizeof(*hdr)) ? -EFAULT : 0;
		if (!ret)
			ret = gc02m0_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32)) ? -EFAULT : 0;
		if (!ret)
			ret = gc02m0_ioctl(sd, cmd, &stream);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc02m0_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC02M0_XVCLK_FREQ / 1000 / 1000);
}

static void gc02m0_release_reset_locked(struct gc02m0 *gc02m0)
{
	struct gc02m0_reset *reset = gc02m0->reset;

	lockdep_assert_held(&gc02m0_reset_mutex);
	if (!gc02m0->reset_active)
		return;

	gc02m0->reset_active = false;
	if (!--reset->active_users)
		gpiod_set_value_cansleep(reset->gpio, 1);
}

static void gc02m0_put_reset(void *data)
{
	struct gc02m0 *gc02m0 = data;
	struct gc02m0_reset *reset = gc02m0->reset;

	mutex_lock(&gc02m0_reset_mutex);
	gc02m0_release_reset_locked(gc02m0);
	if (!--reset->consumers) {
		list_del(&reset->list);
		kfree(reset);
	}
	gc02m0->reset = NULL;
	mutex_unlock(&gc02m0_reset_mutex);
}

static int gc02m0_get_reset(struct gc02m0 *gc02m0)
{
	struct device *dev = &gc02m0->client->dev;
	struct gc02m0_reset *reset;

	if (IS_ERR_OR_NULL(gc02m0->reset_gpio))
		return 0;

	mutex_lock(&gc02m0_reset_mutex);
	list_for_each_entry(reset, &gc02m0_reset_lines, list) {
		if (reset->gpio == gc02m0->reset_gpio)
			goto found;
	}

	reset = kzalloc(sizeof(*reset), GFP_KERNEL);
	if (!reset) {
		mutex_unlock(&gc02m0_reset_mutex);
		return -ENOMEM;
	}
	reset->gpio = gc02m0->reset_gpio;
	list_add(&reset->list, &gc02m0_reset_lines);
found:
	reset->consumers++;
	gc02m0->reset = reset;
	mutex_unlock(&gc02m0_reset_mutex);

	return devm_add_action_or_reset(dev, gc02m0_put_reset, gc02m0);
}

static int __gc02m0_power_on(struct gc02m0 *gc02m0)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc02m0->client->dev;

	if (!IS_ERR_OR_NULL(gc02m0->pins_default)) {
		ret = pinctrl_select_state(gc02m0->pinctrl,
					   gc02m0->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc02m0->xvclk, GC02M0_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc02m0->xvclk) != GC02M0_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc02m0->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	mutex_lock(&gc02m0_reset_mutex);
	if (gc02m0->reset && !gc02m0->reset->active_users) {
		ret = gpiod_direction_output(gc02m0->reset_gpio, 1);
		if (ret)
			goto unlock_disable_clk;
	}
	ret = regulator_bulk_enable(GC02M0_NUM_SUPPLIES, gc02m0->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto unlock_disable_clk;
	}

	usleep_range(1000, 1100);

	if (gc02m0->reset && !gc02m0->reset->active_users)
		gpiod_set_value_cansleep(gc02m0->reset_gpio, 0);
	usleep_range(500, 1000);
	if (!IS_ERR(gc02m0->pwdn_gpio))
		gpiod_set_value_cansleep(gc02m0->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc02m0_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);
	if (gc02m0->reset && !gc02m0->reset_active) {
		gc02m0->reset->active_users++;
		gc02m0->reset_active = true;
	}
	mutex_unlock(&gc02m0_reset_mutex);
	return 0;

unlock_disable_clk:
	mutex_unlock(&gc02m0_reset_mutex);
	clk_disable_unprepare(gc02m0->xvclk);

	return ret;
}

static void __gc02m0_power_off(struct gc02m0 *gc02m0)
{
	int ret;
	struct device *dev = &gc02m0->client->dev;

	mutex_lock(&gc02m0_reset_mutex);
	if (!IS_ERR(gc02m0->pwdn_gpio))
		gpiod_set_value_cansleep(gc02m0->pwdn_gpio, 0);
	clk_disable_unprepare(gc02m0->xvclk);
	gc02m0_release_reset_locked(gc02m0);
	if (!IS_ERR_OR_NULL(gc02m0->pins_sleep)) {
		ret = pinctrl_select_state(gc02m0->pinctrl,
					   gc02m0->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC02M0_NUM_SUPPLIES, gc02m0->supplies);
	mutex_unlock(&gc02m0_reset_mutex);
}

static int __gc02m0_start_stream(struct gc02m0 *gc02m0)
{
	int ret;

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&gc02m0->ctrl_handler);
	if (ret)
		return ret;

	ret = gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
	ret |= gc02m0_write_reg(gc02m0->client, GC02M0_MODE_SELECT,
				 GC02M0_MODE_STREAMING);
	ret |= gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);

	return ret;
}

static int __gc02m0_stop_stream(struct gc02m0 *gc02m0)
{
	int ret;

	ret = gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);
	ret |= gc02m0_write_reg(gc02m0->client, GC02M0_MODE_SELECT,
				 GC02M0_MODE_SW_STANDBY);
	ret |= gc02m0_write_reg(gc02m0->client, GC02M0_PAGE_SELECT, 0x00);

	return ret;
}

static int gc02m0_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	struct i2c_client *client = gc02m0->client;
	int ret = 0;

	mutex_lock(&gc02m0->mutex);
	on = !!on;
	if (on == gc02m0->streaming && (!on || gc02m0->stream_pm_held))
		goto unlock_and_return;

	if (!gc02m0->stream_pm_held) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			goto unlock_and_return;
		gc02m0->stream_pm_held = true;
	}

	if (on) {
		ret = __gc02m0_start_stream(gc02m0);
		if (ret)
			v4l2_err(sd, "start stream failed while write regs\n");
	} else {
		ret = __gc02m0_stop_stream(gc02m0);
		if (ret)
			v4l2_err(sd, "stop stream failed while write regs\n");
	}

	if (!ret)
		gc02m0->streaming = on;
	if (!on || ret) {
		gc02m0->stream_pm_held = false;
		pm_runtime_put(&client->dev);
	}

unlock_and_return:
	mutex_unlock(&gc02m0->mutex);

	return ret;
}

static int gc02m0_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	struct i2c_client *client = gc02m0->client;
	int ret = 0;

	mutex_lock(&gc02m0->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc02m0->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = gc02m0_write_array(gc02m0->client, gc02m0->cur_mode->reg_list);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}

		gc02m0->cur_vts = gc02m0->cur_mode->vts_def;
		gc02m0->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gc02m0->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc02m0->mutex);

	return ret;
}

static int gc02m0_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc02m0 *gc02m0 = to_gc02m0(sd);

	return __gc02m0_power_on(gc02m0);
}

static int gc02m0_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc02m0 *gc02m0 = to_gc02m0(sd);

	__gc02m0_power_off(gc02m0);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc02m0_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc02m0 *gc02m0 = to_gc02m0(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc02m0_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc02m0->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc02m0->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gc02m0_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = GC02M0_LANES;
	return 0;
}

static int gc02m0_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
					struct v4l2_subdev_frame_interval_enum *fie)
{
	const struct gc02m0_mode *mode;

	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	mode = &supported_modes[fie->index];
	if (fie->code != mode->bus_fmt || fie->width != mode->width ||
	    fie->height != mode->height)
		return -EINVAL;

	fie->interval = mode->max_fps;
	fie->reserved[0] = mode->hdr_mode;
	return 0;
}

static const struct dev_pm_ops gc02m0_pm_ops = {
	SET_RUNTIME_PM_OPS(gc02m0_runtime_suspend,
			   gc02m0_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc02m0_internal_ops = {
	.open = gc02m0_open,
};
#endif

static const struct v4l2_subdev_core_ops gc02m0_core_ops = {
	.s_power = gc02m0_s_power,
	.ioctl = gc02m0_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc02m0_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc02m0_video_ops = {
	.s_stream = gc02m0_s_stream,
	.g_frame_interval = gc02m0_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc02m0_pad_ops = {
	.enum_mbus_code = gc02m0_enum_mbus_code,
	.enum_frame_size = gc02m0_enum_frame_sizes,
	.enum_frame_interval = gc02m0_enum_frame_interval,
	.get_fmt = gc02m0_get_fmt,
	.set_fmt = gc02m0_set_fmt,
	.get_mbus_config = gc02m0_g_mbus_config,
};

static const struct v4l2_subdev_ops gc02m0_subdev_ops = {
	.core	= &gc02m0_core_ops,
	.video	= &gc02m0_video_ops,
	.pad	= &gc02m0_pad_ops,
};

#define DIGITAL_GAIN_BASE 1024
static int gc02m0_set_gain_reg(struct gc02m0 *gc02m0, u32 total_gain)
{
	struct device *dev = &gc02m0->client->dev;
	int ret = 0, i = 0;
	u32 dgain = 0;

	dev_dbg(dev, "total_gain = 0x%04x!\n", total_gain);
	total_gain = clamp_t(u32, total_gain, 0x400, 0x3000);

	for (i = 15; i > 0; i--)
		if (total_gain >= gc02m0_agc_params[i][0])
			break;
	ret = gc02m0_write_reg(gc02m0->client,
			       GC02M0_PAGE_SELECT,	0x00);
	ret |= gc02m0_write_reg(gc02m0->client,
		GC02M0_ANALOG_GAIN_REG, gc02m0_agc_params[i][1]);
	dgain = total_gain * DIGITAL_GAIN_BASE / gc02m0_agc_params[i][0];

	dev_dbg(dev, "AGC_Param[%d][0] = %d dgain = 0x%04x!\n",
		i, gc02m0_agc_params[i][0], dgain);
	ret |= gc02m0_write_reg(gc02m0->client,
		GC02M0_PREGAIN_H_REG,
		dgain >> 8);
	ret |= gc02m0_write_reg(gc02m0->client,
		GC02M0_PREGAIN_L_REG,
		dgain & 0xff);
	return ret;
}

static int gc02m0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc02m0 *gc02m0 = container_of(ctrl->handler,
					     struct gc02m0, ctrl_handler);
	struct i2c_client *client = gc02m0->client;
	s64 max;
	int ret = 0;
	u32 vts = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc02m0->cur_mode->height + ctrl->val - 16;
		__v4l2_ctrl_modify_range(gc02m0->exposure,
					 gc02m0->exposure->minimum, max,
					 gc02m0->exposure->step,
					 gc02m0->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = gc02m0_write_reg(gc02m0->client,
				       GC02M0_PAGE_SELECT, 0x00);
		ret |= gc02m0_write_reg(gc02m0->client,
					 GC02M0_REG_EXPOSURE_H,
					 (ctrl->val >> 8) & 0x3f);
		ret |= gc02m0_write_reg(gc02m0->client,
					 GC02M0_REG_EXPOSURE_L,
					 ctrl->val & 0xff);

		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = gc02m0_set_gain_reg(gc02m0, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + gc02m0->cur_mode->height;
		ret = gc02m0_write_reg(gc02m0->client,
				       GC02M0_PAGE_SELECT, 0x00);
		ret |= gc02m0_write_reg(gc02m0->client, GC02M0_REG_VTS_H,
			(vts >> 8) & 0x3f);
		ret |= gc02m0_write_reg(gc02m0->client, GC02M0_REG_VTS_L,
			vts & 0xff);
		if (!ret)
			gc02m0->cur_vts = vts;
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc02m0_ctrl_ops = {
	.s_ctrl = gc02m0_set_ctrl,
};

static int gc02m0_check_sensor_id(struct gc02m0 *gc02m0,
				  struct i2c_client *client)
{
	struct device *dev = &gc02m0->client->dev;
	u8 pid, ver = 0x00;
	int ret;
	unsigned short id;

	ret = gc02m0_read_reg(client, GC02M0_REG_CHIP_ID_H, &pid);
	if (ret) {
		dev_err(dev, "Read chip ID H register error\n");
		return ret;
	}

	ret = gc02m0_read_reg(client, GC02M0_REG_CHIP_ID_L, &ver);
	if (ret) {
		dev_err(dev, "Read chip ID L register error\n");
		return ret;
	}

	id = SENSOR_ID(pid, ver);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "detected gc%04x sensor\n", id);

	return 0;
}

static int gc02m0_configure_regulators(struct gc02m0 *gc02m0)
{
	unsigned int i;

	for (i = 0; i < GC02M0_NUM_SUPPLIES; i++)
		gc02m0->supplies[i].supply = gc02m0_supply_names[i];

	return devm_regulator_bulk_get(&gc02m0->client->dev,
				       GC02M0_NUM_SUPPLIES,
				       gc02m0->supplies);
}

static int gc02m0_parse_of(struct gc02m0 *gc02m0)
{
	struct device *dev = &gc02m0->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}
	fwnode = of_fwnode_handle(endpoint);
	rval = fwnode_property_read_u32_array(fwnode, "data-lanes", NULL, 0);
	of_node_put(endpoint);
	if (rval <= 0) {
		dev_warn(dev, " Get mipi lane num failed!\n");
		return -1;
	}

	gc02m0->lane_num = rval;
	if (gc02m0->lane_num == 1) {
		gc02m0->cur_mode = &supported_modes[0];
		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		gc02m0->pixel_rate = GC02M0_MIPI_LINK_FREQ * 2U * gc02m0->lane_num / 10U;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 gc02m0->lane_num, gc02m0->pixel_rate);
	} else {
		dev_err(dev, "unsupported lane_num(%d)\n", gc02m0->lane_num);
		return -1;
	}
	return 0;
}

static int gc02m0_initialize_controls(struct gc02m0 *gc02m0)
{
	const struct gc02m0_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	struct device *dev = &gc02m0->client->dev;

	dev_info(dev, "Enter %s(%d) !\n", __func__, __LINE__);
	handler = &gc02m0->ctrl_handler;
	mode = gc02m0->cur_mode;
	gc02m0->cur_vts = mode->vts_def;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &gc02m0->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, GC02M0_PIXEL_RATE, 1, GC02M0_PIXEL_RATE);

	h_blank = mode->hts_def - mode->width;
	gc02m0->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc02m0->hblank)
		gc02m0->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc02m0->vblank = v4l2_ctrl_new_std(handler, &gc02m0_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
				GC02M0_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 16;
	gc02m0->exposure = v4l2_ctrl_new_std(handler, &gc02m0_ctrl_ops,
					     V4L2_CID_EXPOSURE, GC02M0_EXPOSURE_MIN,
				exposure_max, GC02M0_EXPOSURE_STEP,
				mode->exp_def);

	gc02m0->anal_gain = v4l2_ctrl_new_std(handler, &gc02m0_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN, GC02M0_GAIN_MIN,
				GC02M0_GAIN_MAX, GC02M0_GAIN_STEP,
				GC02M0_GAIN_DEFAULT);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc02m0->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc02m0->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc02m0_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc02m0 *gc02m0;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	gc02m0 = devm_kzalloc(dev, sizeof(*gc02m0), GFP_KERNEL);
	if (!gc02m0)
		return -ENOMEM;

	gc02m0->sensor_name = of_device_get_match_data(dev);
	if (!gc02m0->sensor_name)
		gc02m0->sensor_name = id ? id->name : GC02M0_NAME;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc02m0->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gc02m0->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gc02m0->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gc02m0->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	gc02m0->client = client;
	gc02m0->cur_mode = &supported_modes[0];

	gc02m0->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc02m0->xvclk))
		return dev_err_probe(dev, PTR_ERR(gc02m0->xvclk),
				     "Failed to get xvclk\n");

	gc02m0->reset_gpio = devm_gpiod_get(dev, "reset",
					    GPIOD_ASIS | GPIOD_FLAGS_BIT_NONEXCLUSIVE);
	if (IS_ERR(gc02m0->reset_gpio)) {
		if (PTR_ERR(gc02m0->reset_gpio) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_warn(dev, "Failed to get reset-gpios\n");
	}

	ret = gc02m0_get_reset(gc02m0);
	if (ret)
		return ret;

	gc02m0->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc02m0->pwdn_gpio)) {
		if (PTR_ERR(gc02m0->pwdn_gpio) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_warn(dev, "Failed to get pwdn-gpios\n");
	}

	ret = gc02m0_parse_of(gc02m0);
	if (ret != 0)
		return -EINVAL;

	gc02m0->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc02m0->pinctrl)) {
		gc02m0->pins_default =
			pinctrl_lookup_state(gc02m0->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc02m0->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc02m0->pins_sleep =
			pinctrl_lookup_state(gc02m0->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc02m0->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = gc02m0_configure_regulators(gc02m0);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&gc02m0->mutex);

	sd = &gc02m0->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc02m0_subdev_ops);
	ret = gc02m0_initialize_controls(gc02m0);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc02m0_power_on(gc02m0);
	if (ret)
		goto err_free_handler;

	ret = gc02m0_check_sensor_id(gc02m0, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc02m0_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc02m0->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc02m0->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc02m0->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc02m0->module_index, facing,
		 gc02m0->sensor_name, dev_name(sd->dev));
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
	__gc02m0_power_off(gc02m0);
err_free_handler:
	v4l2_ctrl_handler_free(&gc02m0->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc02m0->mutex);

	return ret;
}

static void gc02m0_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc02m0 *gc02m0 = to_gc02m0(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc02m0->ctrl_handler);
	mutex_destroy(&gc02m0->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc02m0_power_off(gc02m0);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc02m0_of_match[] = {
	{ .compatible = "galaxycore,gc02m0", .data = "gc02m0" },
	{ .compatible = "galaxycore,gc02m1", .data = "gc02m1" },
	{},
};
MODULE_DEVICE_TABLE(of, gc02m0_of_match);
#endif

static const struct i2c_device_id gc02m0_match_id[] = {
	{ "gc02m0", 0 },
	{ "gc02m1", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, gc02m0_match_id);

static struct i2c_driver gc02m0_i2c_driver = {
	.driver = {
		.name = GC02M0_NAME,
		.pm = &gc02m0_pm_ops,
		.of_match_table = of_match_ptr(gc02m0_of_match),
	},
	.probe		= &gc02m0_probe,
	.remove		= &gc02m0_remove,
	.id_table	= gc02m0_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc02m0_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc02m0_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("GalaxyCore GC02M0 and GC02M1 sensor driver");
MODULE_LICENSE("GPL");
