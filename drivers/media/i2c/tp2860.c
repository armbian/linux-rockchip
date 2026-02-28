// SPDX-License-Identifier: GPL-2.0
/*
 * tp2860/tp9951 driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 */

//#define DEBUG
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
#include <linux/sched.h>
#include <linux/kthread.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include <linux/platform_device.h>
#include <linux/input.h>
#include "tp2860.h"

#define DRIVER_VERSION				KERNEL_VERSION(0, 0x01, 0x1)
#define TP2860_TEST_PATTERN			0
#define TP2860_XVCLK_FREQ			27000000
#define TP2860_LINK_FREQ_74M			(74250000UL)
#define TP2860_LINK_FREQ_148M			(148500000UL)
#define TP2860_LINK_FREQ_297M			(297000000UL)
#define TP2860_LANES				2
#define TP2860_BITS_PER_SAMPLE			8
#define TP2860_NAME				"tp2860"
#define OF_CAMERA_PINCTRL_STATE_DEFAULT		"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP		"rockchip,camera_sleep"

static const char *const tp2860_supply_names[] = {
	"dovdd",		/* Digital I/O power */
	"avdd",			/* Analog power */
	"dvdd",			/* Digital power */
};
#define TP2860_NUM_SUPPLIES ARRAY_SIZE(tp2860_supply_names)

enum{
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
	MIPI_PAGE = 8,
};

#define STD_TVI 0
#define STD_HDA 1
#define PAGE_REG			0x40
#define CVBS_960H			(1) //1->960H 0->720H
#define TECHPOINT_TEST_PATTERN	0
#define TP2860_LOOP_COUNT	10

enum tp2860_support_mipi_lane {
	MIPI_2LANE,
	MIPI_1LANE,
};

enum tp2860_support_reso {
	TP2860_CVSTD_720P_60 = 0,
	TP2860_CVSTD_720P_50,
	TP2860_CVSTD_1080P_30,
	TP2860_CVSTD_1080P_25,
	TP2860_CVSTD_720P_30,
	TP2860_CVSTD_720P_25,
	TP2860_CVSTD_SD,
	TP2860_CVSTD_OTHER,
	TP2860_CVSTD_720P_275,
	TP2860_CVSTD_QHD30,	//960×540 only support with 2lane mode
	TP2860_CVSTD_QHD25,	 //960×540 only support with 2lane mode
	TP2860_CVSTD_PAL,
	TP2860_CVSTD_NTSC,
	TP2860_CVSTD_UVGA25,  //1280x960p25, must use with MIPI_4CH4LANE_445M
	TP2860_CVSTD_UVGA30,  //1280x960p30, must use with MIPI_4CH4LANE_445M
	TP2860_CVSTD_A_UVGA30,	//HDA 1280x960p30, must use with MIPI_4CH4LANE_378M
	TP2860_CVSTD_F_UVGA30,	//FH 1280x960p30, 1800x1000
	TP2860_CVSTD_HD30864, //total 1600x900 86.4M
	TP2860_CVSTD_HD30HDR, //special 720p30 with ISX019/SC120AT,total 1650x900
	TP2860_CVSTD_1080P_60,//only support with 2lane mode
	TP2860_CVSTD_1080P_50,//only support with 2lane mode
	TP2860_CVSTD_1080P_28,
	TP2860_CVSTD_1080P_275,
};

/*sensor mode*/
enum techpoint_support_reso {
	TECHPOINT_S_RESO_720P_25 = 0,
	TECHPOINT_S_RESO_720P_30,
	TECHPOINT_S_RESO_1080P_25,
	TECHPOINT_S_RESO_1080P_30,
	TECHPOINT_S_RESO_SD,
	TECHPOINT_S_RESO_PAL,
	TECHPOINT_S_RESO_NTSC,
	TECHPOINT_S_RESO_NUMS,
};

struct regval {
	u8 addr;
	u8 val;
};

struct tp2860_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 mipi_freq_idx;
	u32 bpp;
	const struct regval *common_reg_list;
	const struct regval *mipi_reg_list;
	int common_reg_size;
	int mipi_reg_size;
	u32 vc[PAD1];
	enum techpoint_support_reso reso;
};

struct tp2860 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*power_gpio;

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;
	struct regulator_bulk_data supplies[TP2860_NUM_SUPPLIES];

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	bool			power_on;
	const struct tp2860_mode *cur_mode;

	u32			module_index;
	u32			cfg_num;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	bool			lost_video_status;
	struct task_struct	*detect_thread;
	u8			detect_status[PAD1];
	bool			do_reset;
	int			streaming;
};

#define FLAG_LOSS				(0x1 << 7)
#define FLAG_V_LOCKED			(0x1 << 6)
#define FLAG_H_LOCKED			(0x1 << 5)
#define FLAG_CARRIER_PLL_LOCKED	(0x1 << 4)
#define FLAG_VIDEO_DETECTED		(0x1 << 3)
#define FLAG_EQ_SD_DETECTED		(0x1 << 2)
#define FLAG_PROGRESSIVE		(0x1 << 1)
#define FLAG_NO_CARRIER			(0x1 << 0)
#define FLAG_LOCKED		(FLAG_V_LOCKED | FLAG_H_LOCKED | FLAG_CARRIER_PLL_LOCKED)

enum {
	VIDEO_UNPLUG,
	VIDEO_IN,
	VIDEO_LOCKED,
	VIDEO_UNLOCK
};

#define to_tp2860(sd) container_of(sd, struct tp2860, subdev)

static struct tp2860_mode supported_modes[] = {
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 0,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_1080P_25,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 0,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_1080P_30,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 1280,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 1,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_720P_25,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 1280,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 1,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_720P_30,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 960,
		.height = 576,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 2,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_PAL,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 960,
		.height = 480,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.common_reg_list = NULL,
		.common_reg_size = 0,
		.mipi_reg_list = NULL,
		.mipi_reg_size = 0,
		.mipi_freq_idx = 2,
		.bpp = 8,
		.vc[PAD0] = 0,
		.reso = TECHPOINT_S_RESO_NTSC,
	},
};

static const s64 link_freq_items[] = {
	TP2860_LINK_FREQ_297M,
	TP2860_LINK_FREQ_148M,
	TP2860_LINK_FREQ_74M
};

/* sensor register write */
static int tp2860_write_reg(struct i2c_client *client, u8 reg, u8 val)
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
	if (ret >= 0) {
		usleep_range(300, 400);
		return 0;
	}

	dev_err(&client->dev,
		"tp2860 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int tp2860_write_array(struct i2c_client *client,
				  const struct regval *regs, int size)
{
	int i, ret = 0;

	i = 0;
	while (i < size) {
		ret = tp2860_write_reg(client, regs[i].addr, regs[i].val);
		if (ret) {
			dev_err(&client->dev, "%s failed !\n", __func__);
			break;
		}
		i++;
	}

	return ret;
}

/* sensor register read */
static int tp2860_read_reg(struct i2c_client *client, u8 reg, u8 *val)
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
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	dev_err(&client->dev, "tp2860 read reg(0x%x) failed !\n", reg);

	return ret;
}

static int tp2860_get_reso_dist(const struct tp2860_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct tp2860_mode *
tp2860_find_best_fit(struct tp2860 *tp2860,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < tp2860->cfg_num; i++) {
		dist = tp2860_get_reso_dist(&supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist <= cur_best_fit_dist) &&
			supported_modes[i].bus_fmt == framefmt->code) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int tp2860_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	const struct tp2860_mode *mode;
	u64 pixel_rate;

	mutex_lock(&tp2860->mutex);

	dev_info(&tp2860->client->dev, "@%s, wxh:%dx%d, code:%d\n",
			 __func__, fmt->format.width, fmt->format.height, fmt->format.code);

	mode = tp2860_find_best_fit(tp2860, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_SRGB;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&tp2860->mutex);
		return -ENOTTY;
#endif
	} else {
		tp2860->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(tp2860->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
			      mode->bpp * 2 * TP2860_LANES;
		__v4l2_ctrl_s_ctrl_int64(tp2860->pixel_rate, pixel_rate);
		dev_info(&tp2860->client->dev, "mipi_freq_idx %d, pixel_rate %lld\n",
				 mode->mipi_freq_idx, pixel_rate);
	}

	mutex_unlock(&tp2860->mutex);
	return 0;
}

static int tp2860_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	struct i2c_client *client = tp2860->client;
	const struct tp2860_mode *mode = tp2860->cur_mode;

	mutex_lock(&tp2860->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_state_get_format(sd_state, fmt->pad);
#else
		mutex_unlock(&tp2860->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD1) {
			if (mode->reso == TECHPOINT_S_RESO_PAL ||
			    mode->reso == TECHPOINT_S_RESO_NTSC)
				fmt->format.field = V4L2_FIELD_INTERLACED;
			fmt->reserved[0] = mode->vc[PAD0];
		}
	}
	mutex_unlock(&tp2860->mutex);

	dev_dbg(&client->dev, "%s: %x %dx%d\n",
		__func__, fmt->format.code,
		fmt->format.width, fmt->format.height);

	return 0;
}


static int tp2860_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct tp2860 *tp2860 = to_tp2860(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = tp2860->cur_mode->bus_fmt;

	return 0;
}

static int tp2860_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct tp2860 *tp2860 = to_tp2860(sd);

	if (fse->index >= tp2860->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;
	return 0;
}

static int tp2860_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				 struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;
	cfg->bus.mipi_csi2.num_data_lanes = TP2860_LANES;

	return 0;
}

static int tp2860_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	const struct tp2860_mode *mode = tp2860->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void tp2860_get_module_inf(struct tp2860 *tp2860,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, TP2860_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, tp2860->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, tp2860->len_name, sizeof(inf->base.lens));
}

static long tp2860_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		tp2860_get_module_inf(tp2860, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_START_STREAM_SEQ:
		*(int *)arg = RKMODULE_START_STREAM_FRONT;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long tp2860_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret = 0;
	int *stream_seq;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = tp2860_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = tp2860_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_GET_START_STREAM_SEQ:
		stream_seq = kzalloc(sizeof(*stream_seq), GFP_KERNEL);
		if (!stream_seq) {
			ret = -ENOMEM;
			return ret;
		}

		ret = tp2860_ioctl(sd, cmd, stream_seq);
		if (!ret) {
			ret = copy_to_user(up, stream_seq, sizeof(*stream_seq));
			if (ret)
				return -EFAULT;
		}
		kfree(stream_seq);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int tp2860_get_channel_input_status(struct tp2860 *tp2860, u8 ch)
{
	struct i2c_client *client = tp2860->client;
	u8 val = 0;

	mutex_lock(&tp2860->mutex);
	tp2860_write_reg(client, 0x40, ch);
	tp2860_read_reg(client, 0x01, &val);
	mutex_unlock(&tp2860->mutex);
	dev_dbg(&client->dev, "input_status ch %d : %x\n", ch, val);

	return (val & 0x80) ? 0 : 1;
}

static int tp2860_get_all_input_status(struct tp2860 *tp2860, u8 *detect_status)
{
	struct i2c_client *client = tp2860->client;
	u8 val = 0, i;

	for (i = 0; i < PAD1; i++) {
		tp2860_write_reg(client, 0x40, i);
		tp2860_read_reg(client, 0x01, &val);
		detect_status[i] = tp2860_get_channel_input_status(tp2860, i);
	}

	return 0;
}

static int tp2860_set_decoder_mode(struct i2c_client *client, int ch, int status)
{
	u8 val = 0;

	tp2860_write_reg(client, 0x40, ch);
	tp2860_read_reg(client, 0x26, &val);
	if (status)
		val |= 0x1;
	else
		val &= ~0x1;
	tp2860_write_reg(client, 0x26, val);

	return 0;
}

static int detect_thread_function(void *data)
{
	struct tp2860 *tp2860 = (struct tp2860 *) data;
	struct i2c_client *client = tp2860->client;
	u8 detect_status = 0;
	int i, need_reset_wait = -1;

	if (tp2860->power_on) {
		tp2860_get_all_input_status(tp2860, tp2860->detect_status);
		for (i = 0; i < PAD1; i++)
			tp2860_set_decoder_mode(client, i, tp2860->detect_status[i]);
		tp2860->do_reset = 0;
	}

	while (!kthread_should_stop()) {
		if (tp2860->power_on) {
			for (i = 0; i < PAD1; i++) {
				detect_status = tp2860_get_channel_input_status(tp2860, i);
				if (detect_status != tp2860->detect_status[i]) {
					if (!detect_status)
						dev_info(&client->dev,
							"detect channel %d video plug out\n", i);
					else
						dev_info(&client->dev,
							"detect channel %d video plug in\n", i);
					tp2860->detect_status[i] = detect_status;
					tp2860_set_decoder_mode(client, i, detect_status);
					need_reset_wait = 5;
				}
			}
			if (need_reset_wait > 0) {
				need_reset_wait--;
			} else if (need_reset_wait == 0) {
				need_reset_wait = -1;
				tp2860->do_reset = 1;
				dev_info(&client->dev,
					"trigger reset time up\n");
			}
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(200));
	}
	return 0;
}

static int __maybe_unused detect_thread_start(struct tp2860 *tp2860)
{
	int ret = 0;
	struct i2c_client *client = tp2860->client;

	dev_info(&client->dev, "Detect thread started\n");
	tp2860->detect_thread = kthread_create(detect_thread_function,
				tp2860, "tp2860_kthread");
	if (IS_ERR(tp2860->detect_thread)) {
		dev_err(&client->dev, "kthread_create tp2860_kthread failed\n");
		ret = PTR_ERR(tp2860->detect_thread);
		tp2860->detect_thread = NULL;
		return ret;
	}
	wake_up_process(tp2860->detect_thread);
	return ret;
}

static int __maybe_unused detect_thread_stop(struct tp2860 *tp2860)
{
	struct i2c_client *client = tp2860->client;

	dev_info(&client->dev, "Detect thread stopped\n");
	if (tp2860->detect_thread)
		kthread_stop(tp2860->detect_thread);
	tp2860->detect_thread = NULL;
	return 0;
}

static void tp2860_set_mipi_out(struct i2c_client *client,
				enum techpoint_support_reso reso,
				unsigned char lane)
{
	u8 tmp = 0;
	//mipi setting
	tp2860_write_reg(client, PAGE_REG, 0x08); //select MIPI page
	tp2860_write_reg(client, 0x28, 0x02);	 //stream off
	tp2860_write_reg(client, 0x02, 0x7d);
	tp2860_write_reg(client, 0x03, 0x75);
	tp2860_write_reg(client, 0x04, 0x75);
	tp2860_write_reg(client, 0x13, 0xef);
	tp2860_write_reg(client, 0x20, 0x00);
	tp2860_write_reg(client, 0x23, 0x9e);

	if (lane == MIPI_1LANE) {
		tp2860_write_reg(client, 0x21, 0x11);

		if (TECHPOINT_S_RESO_1080P_30 == reso || TECHPOINT_S_RESO_1080P_25 == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x00);
			tp2860_write_reg(client, 0x15, 0x02);

			tp2860_write_reg(client, 0x2a, 0x08);
			tp2860_write_reg(client, 0x2b, 0x06);
			tp2860_write_reg(client, 0x2c, 0x12);
			tp2860_write_reg(client, 0x2e, 0x0a);
		} else if (TECHPOINT_S_RESO_720P_30 == reso || TECHPOINT_S_RESO_720P_25 == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x00);
			tp2860_write_reg(client, 0x15, 0x12);

			tp2860_write_reg(client, 0x2a, 0x04);
			tp2860_write_reg(client, 0x2b, 0x03);
			tp2860_write_reg(client, 0x2c, 0x0a);
			tp2860_write_reg(client, 0x2e, 0x02);
		} else if (TECHPOINT_S_RESO_NTSC == reso || TECHPOINT_S_RESO_PAL == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x51);
			tp2860_write_reg(client, 0x15, 0x07);

			tp2860_write_reg(client, 0x2a, 0x02);
			tp2860_write_reg(client, 0x2b, 0x01);
			tp2860_write_reg(client, 0x2c, 0x06);
			tp2860_write_reg(client, 0x2e, 0x02);
		}
	} else {	// 2 lane
		tp2860_write_reg(client, 0x21, 0x12);

		if (TECHPOINT_S_RESO_1080P_30 == reso || TECHPOINT_S_RESO_1080P_25 == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x41);
			tp2860_write_reg(client, 0x15, 0x02);

			tp2860_write_reg(client, 0x2a, 0x04);
			tp2860_write_reg(client, 0x2b, 0x03);
			tp2860_write_reg(client, 0x2c, 0x0a);
			tp2860_write_reg(client, 0x2e, 0x02);
		} else if (TECHPOINT_S_RESO_720P_30 == reso || TECHPOINT_S_RESO_720P_25 == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x41);
			tp2860_write_reg(client, 0x15, 0x12);

			tp2860_write_reg(client, 0x2a, 0x02);
			tp2860_write_reg(client, 0x2b, 0x01);
			tp2860_write_reg(client, 0x2c, 0x06);
			tp2860_write_reg(client, 0x2e, 0x02);
		} else if (TECHPOINT_S_RESO_NTSC == reso || TECHPOINT_S_RESO_PAL == reso) {
			tp2860_write_reg(client, 0x12, 0x54);
			tp2860_write_reg(client, 0x14, 0x62);
			tp2860_write_reg(client, 0x15, 0x07);

			tp2860_write_reg(client, 0x2a, 0x02);
			tp2860_write_reg(client, 0x2b, 0x00);
			tp2860_write_reg(client, 0x2c, 0x04);
			tp2860_write_reg(client, 0x2e, 0x02);
		}
	}

	tp2860_write_reg(client, PAGE_REG, 0x00); //back to decoder page
	tp2860_read_reg(client, 0x06, &tmp); //PLL reset
	tp2860_write_reg(client, 0x06, 0x80 | tmp);

	tp2860_write_reg(client, PAGE_REG, 0x08); //back to mipi page

	tp2860_read_reg(client, 0x14, &tmp); //PLL reset
	tp2860_write_reg(client, 0x14, 0x80 | tmp);
	tp2860_write_reg(client, 0x14, tmp);

	/* Enable MIPI CSI2 output */
	tp2860_write_reg(client, 0x28, 0x02);	 //stream off
	//tp2860_write_reg(client, 0x28, 0x00);	 //stream on
	tp2860_write_reg(client, PAGE_REG, 0x00);	 //back to decoder page
}

static int tp2860_set_channel_reso(struct i2c_client *client, int ch,
			    enum techpoint_support_reso reso)
{
	int val = reso;

	tp2860_write_reg(client, 0x40, 0x00);	//select decoder page
	tp2860_write_reg(client, 0x06, 0x12);	//default value
	tp2860_write_reg(client, 0x42, 0x00);	//common setting for all format
	tp2860_write_reg(client, 0x4e, 0x00);	//common setting for MIPI output
	tp2860_write_reg(client, 0x54, 0x00);	//common setting for MIPI output
	tp2860_write_reg(client, 0x41, ch);	//video MUX select

	switch (val) {
	case TECHPOINT_S_RESO_1080P_25:		// FHD25
	default:
		dev_info(&client->dev, "set channel 1080P_25\n");
		tp2860_write_reg(client, 0x02, 0x40);
		tp2860_write_reg(client, 0x07, 0xc0);
		tp2860_write_reg(client, 0x0b, 0xc0);
		tp2860_write_reg(client, 0x0c, 0x03);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x03);
		tp2860_write_reg(client, 0x16, 0xd2);
		tp2860_write_reg(client, 0x17, 0x80);
		tp2860_write_reg(client, 0x18, 0x29);
		tp2860_write_reg(client, 0x19, 0x38);
		tp2860_write_reg(client, 0x1a, 0x47);
		tp2860_write_reg(client, 0x1c, 0x0a);	//1920*1080, 25fps
		tp2860_write_reg(client, 0x1d, 0x50);

		tp2860_write_reg(client, 0x20, 0x30);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x60);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x30);
		tp2860_write_reg(client, 0x2e, 0x70);

		tp2860_write_reg(client, 0x30, 0x48);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x2e);
		tp2860_write_reg(client, 0x33, 0x90);
		tp2860_write_reg(client, 0x35, 0x05);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x1C);
		if (STD_HDA) {
			tp2860_write_reg(client, 0x02, 0x44);
			tp2860_write_reg(client, 0x0d, 0x73);
			tp2860_write_reg(client, 0x15, 0x01);
			tp2860_write_reg(client, 0x16, 0xf0);
			tp2860_write_reg(client, 0x18, 0x2a);
			tp2860_write_reg(client, 0x20, 0x3c);
			tp2860_write_reg(client, 0x21, 0x46);
			tp2860_write_reg(client, 0x25, 0xfe);
			tp2860_write_reg(client, 0x26, 0x0d);
			tp2860_write_reg(client, 0x2c, 0x3a);
			tp2860_write_reg(client, 0x2d, 0x54);
			tp2860_write_reg(client, 0x2e, 0x40);
			tp2860_write_reg(client, 0x30, 0xa5);
			tp2860_write_reg(client, 0x31, 0x86);
			tp2860_write_reg(client, 0x32, 0xfb);
			tp2860_write_reg(client, 0x33, 0x60);
		}

		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	case TECHPOINT_S_RESO_1080P_30:		// FHD30
		dev_info(&client->dev, "set channel 1080P_30\n");
		tp2860_write_reg(client, 0x02, 0x40);
		tp2860_write_reg(client, 0x07, 0xc0);
		tp2860_write_reg(client, 0x0b, 0xc0);
		tp2860_write_reg(client, 0x0c, 0x03);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x03);
		tp2860_write_reg(client, 0x16, 0xd2);
		tp2860_write_reg(client, 0x17, 0x80);
		tp2860_write_reg(client, 0x18, 0x29);
		tp2860_write_reg(client, 0x19, 0x38);
		tp2860_write_reg(client, 0x1a, 0x47);
		tp2860_write_reg(client, 0x1c, 0x08);  //1920*1080, 30fps
		tp2860_write_reg(client, 0x1d, 0x98);  //

		tp2860_write_reg(client, 0x20, 0x30);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x60);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x30);
		tp2860_write_reg(client, 0x2e, 0x70);

		tp2860_write_reg(client, 0x30, 0x48);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x2e);
		tp2860_write_reg(client, 0x33, 0x90);

		tp2860_write_reg(client, 0x35, 0x05);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x1C);

		if (STD_HDA) { //AHD1080p30 extra
			tp2860_write_reg(client, 0x02, 0x44);
			tp2860_write_reg(client, 0x0d, 0x72);

			tp2860_write_reg(client, 0x15, 0x01);
			tp2860_write_reg(client, 0x16, 0xf0);
			tp2860_write_reg(client, 0x18, 0x2a);

			tp2860_write_reg(client, 0x20, 0x38);
			tp2860_write_reg(client, 0x21, 0x46);

			tp2860_write_reg(client, 0x25, 0xfe);
			tp2860_write_reg(client, 0x26, 0x0d);

			tp2860_write_reg(client, 0x2c, 0x3a);
			tp2860_write_reg(client, 0x2d, 0x54);
			tp2860_write_reg(client, 0x2e, 0x40);

			tp2860_write_reg(client, 0x30, 0xa5);
			tp2860_write_reg(client, 0x31, 0x95);
			tp2860_write_reg(client, 0x32, 0xe0);
			tp2860_write_reg(client, 0x33, 0x60);
		}

		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	case TECHPOINT_S_RESO_720P_25:
		dev_info(&client->dev, "set channel 720P_25\n");
		tp2860_write_reg(client, 0x02, 0x42);
		tp2860_write_reg(client, 0x07, 0xc0);
		tp2860_write_reg(client, 0x0b, 0xc0);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x13);
		tp2860_write_reg(client, 0x16, 0x15);
		tp2860_write_reg(client, 0x17, 0x00);
		tp2860_write_reg(client, 0x18, 0x19);
		tp2860_write_reg(client, 0x19, 0xd0);
		tp2860_write_reg(client, 0x1a, 0x25);
		tp2860_write_reg(client, 0x1c, 0x07);//1280*720, 25fps
		tp2860_write_reg(client, 0x1d, 0xbc);//1280*720, 25fps

		tp2860_write_reg(client, 0x20, 0x30);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x60);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x30);
		tp2860_write_reg(client, 0x2e, 0x70);

		tp2860_write_reg(client, 0x30, 0x48);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x2e);
		tp2860_write_reg(client, 0x33, 0x90);
		tp2860_write_reg(client, 0x35, 0x25);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x18);
		if (STD_HDA) {
			tp2860_write_reg(client, 0x02, 0x46);
			tp2860_write_reg(client, 0x0d, 0x71);
			tp2860_write_reg(client, 0x18, 0x1b);
			tp2860_write_reg(client, 0x20, 0x40);
			tp2860_write_reg(client, 0x21, 0x46);
			tp2860_write_reg(client, 0x25, 0xfe);
			tp2860_write_reg(client, 0x26, 0x01);
			tp2860_write_reg(client, 0x2c, 0x3a);
			tp2860_write_reg(client, 0x2d, 0x5a);
			tp2860_write_reg(client, 0x2e, 0x40);
			tp2860_write_reg(client, 0x30, 0x9e);
			tp2860_write_reg(client, 0x31, 0x20);
			tp2860_write_reg(client, 0x32, 0x10);
			tp2860_write_reg(client, 0x33, 0x90);
		}

		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	case TECHPOINT_S_RESO_720P_30:
		dev_info(&client->dev, "set channel 720P_30\n");
		tp2860_write_reg(client, 0x02, 0x42);
		tp2860_write_reg(client, 0x07, 0xc0);
		tp2860_write_reg(client, 0x0b, 0xc0);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x13);
		tp2860_write_reg(client, 0x16, 0x15);
		tp2860_write_reg(client, 0x17, 0x00);
		tp2860_write_reg(client, 0x18, 0x1a);
		tp2860_write_reg(client, 0x19, 0xd0);
		tp2860_write_reg(client, 0x1a, 0x25);
		tp2860_write_reg(client, 0x1c, 0x06); //1280*720, 30fps
		tp2860_write_reg(client, 0x1d, 0x72); //1280*720, 30fps

		tp2860_write_reg(client, 0x20, 0x30);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x60);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x30);
		tp2860_write_reg(client, 0x2e, 0x70);

		tp2860_write_reg(client, 0x30, 0x48);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x2e);
		tp2860_write_reg(client, 0x33, 0x90);
		tp2860_write_reg(client, 0x35, 0x25);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x18);
		/* AHD720P30 */
		if (STD_HDA) {
			tp2860_write_reg(client, 0x02, 0x46);
			tp2860_write_reg(client, 0x0d, 0x70);
			tp2860_write_reg(client, 0x18, 0x1b);
			tp2860_write_reg(client, 0x20, 0x40);
			tp2860_write_reg(client, 0x21, 0x46);
			tp2860_write_reg(client, 0x25, 0xfe);
			tp2860_write_reg(client, 0x26, 0x01);
			tp2860_write_reg(client, 0x2c, 0x3a);
			tp2860_write_reg(client, 0x2d, 0x5a);
			tp2860_write_reg(client, 0x2e, 0x40);
			tp2860_write_reg(client, 0x30, 0x9d);
			tp2860_write_reg(client, 0x31, 0xca);
			tp2860_write_reg(client, 0x32, 0x01);
			tp2860_write_reg(client, 0x33, 0xd0);
		}

		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	case TECHPOINT_S_RESO_PAL:
#if CVBS_960H
		dev_info(&client->dev, "set channel CVBS_960H\n");

		tp2860_write_reg(client, 0x02, 0x47);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x51);

		tp2860_write_reg(client, 0x15, 0x13);
		tp2860_write_reg(client, 0x16, 0x76);
		tp2860_write_reg(client, 0x17, 0x80);
		tp2860_write_reg(client, 0x18, 0x17);
		tp2860_write_reg(client, 0x19, 0x20);
		tp2860_write_reg(client, 0x1a, 0x17);
		tp2860_write_reg(client, 0x1c, 0x09);
		tp2860_write_reg(client, 0x1d, 0x48);

		tp2860_write_reg(client, 0x20, 0x48);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x37);
		tp2860_write_reg(client, 0x23, 0x3f);

		tp2860_write_reg(client, 0x2b, 0x70);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x64);
		tp2860_write_reg(client, 0x2e, 0x56);

		tp2860_write_reg(client, 0x30, 0x7a);
		tp2860_write_reg(client, 0x31, 0x4a);
		tp2860_write_reg(client, 0x32, 0x4d);
		tp2860_write_reg(client, 0x33, 0xf0);

		tp2860_write_reg(client, 0x35, 0x65);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x04);

#else	 //PAL 720H
		dev_info(&client->dev, "set channel PAL 720H\n");
		tp2860_write_reg(client, 0x02, 0x47);
		tp2860_write_reg(client, 0x06, 0x32);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x51);

		tp2860_write_reg(client, 0x15, 0x03);
		tp2860_write_reg(client, 0x16, 0xf0);
		tp2860_write_reg(client, 0x17, 0xa0);
		tp2860_write_reg(client, 0x18, 0x17);
		tp2860_write_reg(client, 0x19, 0x20);
		tp2860_write_reg(client, 0x1a, 0x15);
		tp2860_write_reg(client, 0x1c, 0x06);
		tp2860_write_reg(client, 0x1d, 0xc0);

		tp2860_write_reg(client, 0x20, 0x48);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x37);
		tp2860_write_reg(client, 0x23, 0x3f);

		tp2860_write_reg(client, 0x2b, 0x70);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x4b);
		tp2860_write_reg(client, 0x2e, 0x56);

		tp2860_write_reg(client, 0x30, 0x7a);
		tp2860_write_reg(client, 0x31, 0x4a);
		tp2860_write_reg(client, 0x32, 0x4d);
		tp2860_write_reg(client, 0x33, 0xfb);

		tp2860_write_reg(client, 0x35, 0x65);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x04);
#endif
		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	case TECHPOINT_S_RESO_NTSC:
#if CVBS_960H
		dev_info(&client->dev, "set channel NTSC CVBS_960H\n");
		tp2860_write_reg(client, 0x02, 0x47);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x13);
		tp2860_write_reg(client, 0x16, 0x60);
		tp2860_write_reg(client, 0x17, 0x80);
		tp2860_write_reg(client, 0x18, 0x12);
		tp2860_write_reg(client, 0x19, 0xf0);
		tp2860_write_reg(client, 0x1a, 0x07);
		tp2860_write_reg(client, 0x1c, 0x09);
		tp2860_write_reg(client, 0x1d, 0x38);

		tp2860_write_reg(client, 0x20, 0x40);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x70);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x68);
		tp2860_write_reg(client, 0x2e, 0x57);

		tp2860_write_reg(client, 0x30, 0x62);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x96);
		tp2860_write_reg(client, 0x33, 0xc0);

		tp2860_write_reg(client, 0x35, 0x65);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x04);
#else
		dev_info(&client->dev, "set channel NTSC 720H\n");
		tp2860_write_reg(client, 0x02, 0x47);
		tp2860_write_reg(client, 0x0c, 0x13);
		tp2860_write_reg(client, 0x0d, 0x50);

		tp2860_write_reg(client, 0x15, 0x03);
		tp2860_write_reg(client, 0x16, 0xd6);
		tp2860_write_reg(client, 0x17, 0xa0);
		tp2860_write_reg(client, 0x18, 0x12);
		tp2860_write_reg(client, 0x19, 0xf0);
		tp2860_write_reg(client, 0x1a, 0x05);
		tp2860_write_reg(client, 0x1c, 0x06);
		tp2860_write_reg(client, 0x1d, 0xb4);

		tp2860_write_reg(client, 0x20, 0x40);
		tp2860_write_reg(client, 0x21, 0x84);
		tp2860_write_reg(client, 0x22, 0x36);
		tp2860_write_reg(client, 0x23, 0x3c);

		tp2860_write_reg(client, 0x2b, 0x70);
		tp2860_write_reg(client, 0x2c, 0x2a);
		tp2860_write_reg(client, 0x2d, 0x4b);
		tp2860_write_reg(client, 0x2e, 0x57);

		tp2860_write_reg(client, 0x30, 0x62);
		tp2860_write_reg(client, 0x31, 0xbb);
		tp2860_write_reg(client, 0x32, 0x96);
		tp2860_write_reg(client, 0x33, 0xcb);

		tp2860_write_reg(client, 0x35, 0x65);
		tp2860_write_reg(client, 0x38, 0x00);
		tp2860_write_reg(client, 0x39, 0x04);
#endif
		tp2860_set_mipi_out(client, reso, MIPI_2LANE);	// 2 lane
		break;
	}

#if TECHPOINT_TEST_PATTERN
	tp2860_write_reg(client, 0x22, 0x80);
#endif
	return 0;
}

static int tp2860_get_channel_reso(struct tp2860 *tp2860, int ch)
{
	u8 detect_fmt = 0xff;
	u8 reso = 0xff;
	u8 cvbs = 0xff;

	tp2860_write_reg(tp2860->client, 0x40, ch);
	tp2860_read_reg(tp2860->client, 0x03, &detect_fmt);
	reso = detect_fmt & 0x7; // CVSTD[2-0]

	switch (reso) {
	case TP2860_CVSTD_1080P_30:
		dev_info(&tp2860->client->dev, "detect channel %d 1080P_30\n", ch);
		return TECHPOINT_S_RESO_1080P_30;
	case TP2860_CVSTD_1080P_25:
		dev_info(&tp2860->client->dev, "detect channel %d 1080P_25\n", ch);
		return TECHPOINT_S_RESO_1080P_25;
	case TP2860_CVSTD_720P_30:
		dev_info(&tp2860->client->dev, "detect channel %d 720P_30\n", ch);
		return TECHPOINT_S_RESO_720P_30;
	case TP2860_CVSTD_720P_25:
		dev_info(&tp2860->client->dev, "detect channel %d 720P_25\n", ch);
		return TECHPOINT_S_RESO_720P_25;
	case TP2860_CVSTD_SD:
		dev_info(&tp2860->client->dev, "detect channel %d SD\n", ch);
		tp2860_read_reg(tp2860->client, 0x01, &cvbs);
		if (cvbs & 0x04) {
			dev_info(&tp2860->client->dev, "detect channel %d PAL\n", ch);
			return TECHPOINT_S_RESO_PAL;
		} else {
			dev_info(&tp2860->client->dev, "detect channel %d NTSC\n", ch);
			return TECHPOINT_S_RESO_NTSC;
		}
	default:
		dev_info(&tp2860->client->dev,
			"detect channel %d is not supported, default 1080P_25\n", ch);
		return TECHPOINT_S_RESO_1080P_25;
	}
	return reso;
}

static __maybe_unused int auto_detect_channel_fmt(struct tp2860 *tp2860)
{
	int ch = 0;
	enum techpoint_support_reso reso = 0xff;

	reso = tp2860_get_channel_reso(tp2860, ch);
	tp2860_set_channel_reso(tp2860->client, ch, reso);

	return 0;
}

static int __tp2860_start_stream(struct tp2860 *tp2860)
{
	int ret;
	struct i2c_client *client = tp2860->client;
	unsigned char status = 0;
	int state = VIDEO_UNPLUG;
	int check_count = TP2860_LOOP_COUNT;

	auto_detect_channel_fmt(tp2860);
	ret = tp2860_write_array(tp2860->client,
		tp2860->cur_mode->mipi_reg_list, tp2860->cur_mode->mipi_reg_size);
	if (ret) {
		dev_err(&client->dev, "%s: mipi_reg_list failed", __func__);
		return ret;
	}
	usleep_range(500 * 1000, 1000 * 1000);
check_continue:

	if (check_count == TP2860_LOOP_COUNT)
		dev_info(&client->dev, "CHECK VIDEO_LOCKED START\n");
	tp2860_write_reg(client, 0x40, 0x00);
	tp2860_read_reg(client, 0x01, &status);

	if (status & FLAG_LOSS) {
		state = VIDEO_UNPLUG;
	} else if (FLAG_LOCKED == (status & FLAG_LOCKED)) {
		/* video locked */
		state = VIDEO_LOCKED;
		//detect_status = 1;
		dev_info(&client->dev, "CHECK VIDEO_LOCKED END:%0x\n", status);
	} else {
		/* video in but unlocked */
		state = VIDEO_IN;
	}

	if (check_count && state != VIDEO_LOCKED) {
		check_count--;
		usleep_range(100000, 120000);
		goto check_continue;
	}
	return 0;
}

static int __tp2860_stop_stream(struct tp2860 *tp2860)
{
	struct i2c_client *client = tp2860->client;

	tp2860_write_reg(client, 0x40, 0x08);
	tp2860_write_reg(client, 0x28, 0x02);
	tp2860_write_reg(client, 0x40, 0x00);
	return 0;
}

static int tp2860_stream(struct v4l2_subdev *sd, int on)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	struct i2c_client *client = tp2860->client;

	dev_info(&client->dev, "s_stream: %d. %dx%d\n", on,
			tp2860->cur_mode->width,
			tp2860->cur_mode->height);
	mutex_lock(&tp2860->mutex);
	on = !!on;
	if (tp2860->streaming == on)
		goto unlock;

	if (on) {
		__tp2860_start_stream(tp2860);
		tp2860_write_reg(client, 0x40, 0x08);
		tp2860_write_reg(client, 0x28, 0x00);
		tp2860_write_reg(client, 0x40, 0x00);
		detect_thread_start(tp2860);
	} else {
		detect_thread_stop(tp2860);
		__tp2860_stop_stream(tp2860);
	}

	tp2860->streaming = on;
unlock:
	mutex_unlock(&tp2860->mutex);
	return 0;
}

static int tp2860_power(struct v4l2_subdev *sd, int on)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	struct i2c_client *client = tp2860->client;
	int ret = 0;

	mutex_lock(&tp2860->mutex);

	/* If the power state is not modified - no work to do. */
	if (tp2860->power_on == !!on)
		goto exit;

	dev_info(&client->dev, "%s: on %d\n", __func__, on);

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto exit;
		}
		tp2860->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		tp2860->power_on = false;
	}

exit:
	mutex_unlock(&tp2860->mutex);

	return ret;
}

static int __tp2860_power_on(struct tp2860 *tp2860)
{
	int ret;
	struct device *dev = &tp2860->client->dev;

	dev_info(dev, "%s enter!\n", __func__);

	if (!IS_ERR_OR_NULL(tp2860->pins_default)) {
		ret = pinctrl_select_state(tp2860->pinctrl,
					   tp2860->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins. ret=%d\n", ret);
	}

	if (!IS_ERR(tp2860->power_gpio)) {
		gpiod_set_value_cansleep(tp2860->power_gpio, 1);
		usleep_range(25*1000, 30*1000);
	}

	usleep_range(1500, 2000);

	ret = clk_set_rate(tp2860->xvclk, TP2860_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(tp2860->xvclk) != TP2860_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched\n");
	ret = clk_prepare_enable(tp2860->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	ret = regulator_bulk_enable(TP2860_NUM_SUPPLIES, tp2860->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(tp2860->reset_gpio)) {
		gpiod_set_value_cansleep(tp2860->reset_gpio, 1);
		usleep_range(10*1000, 20*1000);
		gpiod_set_value_cansleep(tp2860->reset_gpio, 0);
		usleep_range(10*1000, 20*1000);
	}

	usleep_range(10*1000, 20*1000);

	return 0;

disable_clk:
	clk_disable_unprepare(tp2860->xvclk);

err_clk:
	if (!IS_ERR(tp2860->reset_gpio))
		gpiod_set_value_cansleep(tp2860->reset_gpio, 0);

	if (!IS_ERR_OR_NULL(tp2860->pins_sleep))
		pinctrl_select_state(tp2860->pinctrl, tp2860->pins_sleep);

	return ret;
}

static void __tp2860_power_off(struct tp2860 *tp2860)
{
	int ret;
	struct device *dev = &tp2860->client->dev;

	dev_info(dev, "%s enter!\n", __func__);

	if (!IS_ERR(tp2860->reset_gpio))
		gpiod_set_value_cansleep(tp2860->reset_gpio, 0);
	clk_disable_unprepare(tp2860->xvclk);

	if (!IS_ERR_OR_NULL(tp2860->pins_sleep)) {
		ret = pinctrl_select_state(tp2860->pinctrl,
					   tp2860->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}

	if (!IS_ERR(tp2860->power_gpio))
		gpiod_set_value_cansleep(tp2860->power_gpio, 0);
	regulator_bulk_disable(TP2860_NUM_SUPPLIES, tp2860->supplies);
}

static void tp2860_get_init_format(struct tp2860 *tp2860)
{
	enum techpoint_support_reso reso = 0xff;
	int i = 0;

	reso = tp2860_get_channel_reso(tp2860, 0);

	for (i = 0; i < tp2860->cfg_num; i++) {
		if (reso == supported_modes[i].reso) {
			tp2860->cur_mode = &supported_modes[i];
			return;
		}
	}
	tp2860->cur_mode = &supported_modes[0];
}

static int tp2860_initialize_controls(struct tp2860 *tp2860)
{
	const struct tp2860_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u64 pixel_rate;
	int ret;

	handler = &tp2860->ctrl_handler;
	mode = tp2860->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 2);
	if (ret)
		return ret;
	handler->lock = &tp2860->mutex;

	tp2860->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	__v4l2_ctrl_s_ctrl(tp2860->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * TP2860_LANES;
	tp2860->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, pixel_rate,
		1, pixel_rate);


	if (handler->error) {
		ret = handler->error;
		dev_err(&tp2860->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	tp2860->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int tp2860_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2860 *tp2860 = to_tp2860(sd);

	return __tp2860_power_on(tp2860);
}

static int tp2860_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2860 *tp2860 = to_tp2860(sd);

	__tp2860_power_off(tp2860);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int tp2860_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct tp2860 *tp2860 = to_tp2860(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_state_get_format(fh->state, 0);
	const struct tp2860_mode *def_mode = &supported_modes[0];

	mutex_lock(&tp2860->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&tp2860->mutex);
	/* No crop or compose */

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops tp2860_internal_ops = {
	.open = tp2860_open,
};
#endif

static const struct v4l2_subdev_video_ops tp2860_video_ops = {
	.s_stream = tp2860_stream,
};

static const struct v4l2_subdev_pad_ops tp2860_subdev_pad_ops = {
	.enum_mbus_code = tp2860_enum_mbus_code,
	.enum_frame_size = tp2860_enum_frame_sizes,
	.get_fmt = tp2860_get_fmt,
	.set_fmt = tp2860_set_fmt,
	.get_mbus_config = tp2860_g_mbus_config,
	.get_frame_interval = tp2860_g_frame_interval,
};

static const struct v4l2_subdev_core_ops tp2860_core_ops = {
	.s_power = tp2860_power,
	.ioctl = tp2860_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = tp2860_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_ops tp2860_subdev_ops = {
	.core = &tp2860_core_ops,
	.video = &tp2860_video_ops,
	.pad   = &tp2860_subdev_pad_ops,
};

static int check_chip_id(struct i2c_client *client)
{
	int ret = 0;
	struct device *dev = &client->dev;
	u8 chip_id_h = 0, chip_id_l = 0;

	tp2860_write_reg(client, 0x40, 0x0);
	tp2860_read_reg(client, 0xFE, &chip_id_h);
	tp2860_read_reg(client, 0xFF, &chip_id_l);

	if (chip_id_h != 0x28 || chip_id_l != 0x60) {
		dev_err(dev, "expected 0x2860, detected: 0x%0x%0x\n", chip_id_h, chip_id_l);
		ret = -EINVAL;
	} else {
		dev_info(dev, "Found TP2860 sensor, chip id: 0x%0x%0x\n", chip_id_h, chip_id_l);
	}

	return ret;
}

static int tp2860_configure_regulators(struct tp2860 *tp2860)
{
	unsigned int i;

	for (i = 0; i < TP2860_NUM_SUPPLIES; i++)
		tp2860->supplies[i].supply = tp2860_supply_names[i];

	return devm_regulator_bulk_get(&tp2860->client->dev,
				       TP2860_NUM_SUPPLIES,
				       tp2860->supplies);
}


static int tp2860_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct tp2860 *tp2860;
	struct v4l2_subdev *sd;
	__maybe_unused char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	tp2860 = devm_kzalloc(dev, sizeof(*tp2860), GFP_KERNEL);
	if (!tp2860)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &tp2860->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &tp2860->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &tp2860->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &tp2860->len_name);
	if (ret) {
		dev_err(dev, "could not get %s!\n", RKMODULE_CAMERA_LENS_NAME);
		return -EINVAL;
	}

	tp2860->client = client;
	tp2860->cfg_num = ARRAY_SIZE(supported_modes);

	tp2860->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(tp2860->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	tp2860->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(tp2860->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	tp2860->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(tp2860->power_gpio))
		dev_warn(dev, "Failed to get power-gpios\n");

	ret = tp2860_configure_regulators(tp2860);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	tp2860->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(tp2860->pinctrl)) {
		tp2860->pins_default =
			pinctrl_lookup_state(tp2860->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(tp2860->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		tp2860->pins_sleep =
			pinctrl_lookup_state(tp2860->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(tp2860->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	mutex_init(&tp2860->mutex);
	ret = __tp2860_power_on(tp2860);
	if (ret) {
		dev_err(dev, "Failed to power on tp2860\n");
		goto err_destroy_mutex;
	}
	ret = check_chip_id(client);
	if (ret) {
		dev_err(dev, "Failed to check senosr id\n");
		goto err_power_off;
	}

	tp2860_get_init_format(tp2860);
	sd = &tp2860->subdev;
	v4l2_i2c_subdev_init(sd, client, &tp2860_subdev_ops);
	ret = tp2860_initialize_controls(tp2860);
	if (ret) {
		dev_err(dev, "Failed to initialize controls tp2860\n");
		goto err_power_off;
	}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &tp2860_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	tp2860->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &tp2860->pad);
	if (ret < 0)
		goto err_free_handler;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(tp2860->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 tp2860->module_index, facing,
		 TP2860_NAME, dev_name(sd->dev));

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
err_free_handler:
	v4l2_ctrl_handler_free(&tp2860->ctrl_handler);
err_power_off:
	__tp2860_power_off(tp2860);
err_destroy_mutex:
	mutex_destroy(&tp2860->mutex);

	return ret;
}

static void tp2860_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2860 *tp2860 = to_tp2860(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&tp2860->ctrl_handler);
	mutex_destroy(&tp2860->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__tp2860_power_off(tp2860);
	pm_runtime_set_suspended(&client->dev);
}

static const struct dev_pm_ops tp2860_pm_ops = {
	SET_RUNTIME_PM_OPS(tp2860_runtime_suspend,
			   tp2860_runtime_resume, NULL)
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id tp2860_of_match[] = {
	{ .compatible = "tp2860" },
	{},
};
MODULE_DEVICE_TABLE(of, tp2860_of_match);
#endif

static const struct i2c_device_id tp2860_match_id[] = {
	{ "tp2860", 0 },
	{ },
};

static struct i2c_driver tp2860_i2c_driver = {
	.driver = {
		.name = TP2860_NAME,
		.pm = &tp2860_pm_ops,
		.of_match_table = of_match_ptr(tp2860_of_match),
	},
	.probe		= tp2860_probe,
	.remove		= tp2860_remove,
	.id_table	= tp2860_match_id,
};

int tp2860_sensor_mod_init(void)
{
	return i2c_add_driver(&tp2860_i2c_driver);
}

#ifndef CONFIG_VIDEO_REVERSE_IMAGE
device_initcall_sync(tp2860_sensor_mod_init);
#endif

static void __exit tp2860_sensor_mod_exit(void)
{
	i2c_del_driver(&tp2860_i2c_driver);
}

module_exit(tp2860_sensor_mod_exit);

MODULE_AUTHOR("Randy wang<randy.wang@rock-chips.com>");
MODULE_DESCRIPTION("tp2860 sensor driver");
MODULE_LICENSE("GPL");
