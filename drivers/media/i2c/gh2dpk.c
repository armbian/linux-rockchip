// SPDX-License-Identifier: GPL-2.0
/*
 * gh gb-2dpk driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd. Ltd.
 *
 * V0.0X01.0X01 first implementation.
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
#include <linux/rk-preisp.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define GH2DPK_LANES			2
#define GH2DPK_BITS_PER_SAMPLE		10
#define GH2DPK_LINK_FREQ_250		250000000// 500Mbps

#define PIXEL_RATE_WITH_250M_10BIT	(GH2DPK_LINK_FREQ_250 * 2 * \
					GH2DPK_LANES / GH2DPK_BITS_PER_SAMPLE)

#define GH2DPK_XVCLK_FREQ		27000000

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define GH2DPK_REG_VALUE_08BIT		1
#define GH2DPK_REG_VALUE_16BIT		2
#define GH2DPK_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define GH2DPK_NAME			"gh_2dpk"

static const char * const gh2dpk_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define GH2DPK_NUM_SUPPLIES ARRAY_SIZE(gh2dpk_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct gh2dpk_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct gh2dpk {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[GH2DPK_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_frequency;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct gh2dpk_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_gh2dpk(sd) container_of(sd, struct gh2dpk, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval __maybe_unused gh2dpk_global_regs[] = {
	{REG_NULL, 0x00},
};

static const struct regval __maybe_unused gh2dpk_linear_10_1920x1080_30fps_regs[] = {
	{REG_NULL, 0x00},
};

static const struct gh2dpk_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},

		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	GH2DPK_LINK_FREQ_250
};

/* Write registers up to 4 at a time */
static int __maybe_unused gh2dpk_write_reg(struct i2c_client *client, u16 reg,
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

static int __maybe_unused gh2dpk_write_array(struct i2c_client *client,
					     const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gh2dpk_write_reg(client, regs[i].addr,
				       GH2DPK_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int __maybe_unused gh2dpk_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int gh2dpk_get_reso_dist(const struct gh2dpk_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct gh2dpk_mode *
gh2dpk_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = gh2dpk_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int gh2dpk_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	const struct gh2dpk_mode *mode;

	mutex_lock(&gh2dpk->mutex);

	mode = gh2dpk_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gh2dpk->mutex);
		return -ENOTTY;
#endif
	} else {
		gh2dpk->cur_mode = mode;
	}

	mutex_unlock(&gh2dpk->mutex);

	return 0;
}

static int gh2dpk_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	const struct gh2dpk_mode *mode = gh2dpk->cur_mode;

	mutex_lock(&gh2dpk->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gh2dpk->mutex);
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
	mutex_unlock(&gh2dpk->mutex);

	return 0;
}

static int gh2dpk_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = gh2dpk->cur_mode->bus_fmt;

	return 0;
}

static int gh2dpk_enum_frame_sizes(struct v4l2_subdev *sd,
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

static int gh2dpk_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	const struct gh2dpk_mode *mode = gh2dpk->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static int gh2dpk_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = GH2DPK_LANES;

	return 0;
}

static void gh2dpk_get_module_inf(struct gh2dpk *gh2dpk,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, GH2DPK_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, gh2dpk->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gh2dpk->len_name, sizeof(inf->base.lens));
}

static int gh2dpk_get_channel_info(struct gh2dpk *gh2dpk, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gh2dpk->cur_mode->vc[ch_info->index];
	ch_info->width = gh2dpk->cur_mode->width;
	ch_info->height = gh2dpk->cur_mode->height;
	ch_info->bus_fmt = gh2dpk->cur_mode->bus_fmt;
	return 0;
}

static long gh2dpk_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gh2dpk_get_module_inf(gh2dpk, (struct rkmodule_inf *)arg);
		break;

	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = gh2dpk->cur_mode->hdr_mode;
		break;

	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gh2dpk_get_channel_info(gh2dpk, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gh2dpk_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	long ret;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gh2dpk_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;

	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gh2dpk_ioctl(sd, cmd, hdr);
		if (!ret) {
			ret = copy_to_user(up, hdr, sizeof(*hdr));
			if (ret)
				ret = -EFAULT;
		}
		kfree(hdr);
		break;

	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gh2dpk_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __gh2dpk_start_stream(struct gh2dpk *gh2dpk)
{
	int ret;

	dev_info(&gh2dpk->client->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 gh2dpk->cur_mode->width,
		 gh2dpk->cur_mode->height,
		 gh2dpk->cur_fps.denominator / gh2dpk->cur_fps.numerator,
		 gh2dpk->cur_mode->hdr_mode,
		 gh2dpk->cur_vts);

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&gh2dpk->ctrl_handler);
	if (ret)
		return ret;
	if (gh2dpk->has_init_exp && gh2dpk->cur_mode->hdr_mode != NO_HDR) {
		ret = gh2dpk_ioctl(&gh2dpk->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&gh2dpk->init_hdrae_exp);
		if (ret) {
			dev_err(&gh2dpk->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}

	// reset gpio: high -> low
	if (!IS_ERR(gh2dpk->reset_gpio))
		gpiod_set_value_cansleep(gh2dpk->reset_gpio, 0);

	dev_info(&gh2dpk->client->dev, "%s done\n", __func__);
	return 0;
}

static int __gh2dpk_stop_stream(struct gh2dpk *gh2dpk)
{
	gh2dpk->has_init_exp = false;

	// reset gpio: low -> high
	if (!IS_ERR(gh2dpk->reset_gpio))
		gpiod_set_value_cansleep(gh2dpk->reset_gpio, 1);

	dev_info(&gh2dpk->client->dev, "%s done\n", __func__);
	return 0;
}

static int __gh2dpk_power_on(struct gh2dpk *gh2dpk);
static int gh2dpk_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	struct i2c_client *client = gh2dpk->client;
	int ret = 0;

	mutex_lock(&gh2dpk->mutex);
	on = !!on;
	if (on == gh2dpk->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gh2dpk_start_stream(gh2dpk);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gh2dpk_stop_stream(gh2dpk);
		pm_runtime_put(&client->dev);
	}

	gh2dpk->streaming = on;

unlock_and_return:
	mutex_unlock(&gh2dpk->mutex);

	dev_info(&gh2dpk->client->dev, "%s done\n", __func__);
	return ret;
}

static int gh2dpk_s_power(struct v4l2_subdev *sd, int on)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	struct i2c_client *client = gh2dpk->client;
	int ret = 0;

	mutex_lock(&gh2dpk->mutex);

	/* If the power state is not modified - no work to do. */
	if (gh2dpk->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gh2dpk->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gh2dpk->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gh2dpk->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gh2dpk_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GH2DPK_XVCLK_FREQ / 1000 / 1000);
}

static int __gh2dpk_power_on(struct gh2dpk *gh2dpk)
{
	int ret;
	struct device *dev = &gh2dpk->client->dev;

	if (!IS_ERR(gh2dpk->power_gpio))
		gpiod_set_value_cansleep(gh2dpk->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(gh2dpk->pins_default)) {
		ret = pinctrl_select_state(gh2dpk->pinctrl,
					   gh2dpk->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gh2dpk->xvclk, GH2DPK_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gh2dpk->xvclk) != GH2DPK_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gh2dpk->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	ret = regulator_bulk_enable(GH2DPK_NUM_SUPPLIES, gh2dpk->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(gh2dpk->reset_gpio))
		gpiod_set_value_cansleep(gh2dpk->reset_gpio, 1);

	usleep_range(200000, 210000);	// delay 200ms

	return 0;

disable_clk:
	clk_disable_unprepare(gh2dpk->xvclk);

	return ret;
}

static void __gh2dpk_power_off(struct gh2dpk *gh2dpk)
{
	int ret;
	struct device *dev = &gh2dpk->client->dev;

	clk_disable_unprepare(gh2dpk->xvclk);

	if (!IS_ERR(gh2dpk->reset_gpio))
		gpiod_set_value_cansleep(gh2dpk->reset_gpio, 1);

	if (!IS_ERR_OR_NULL(gh2dpk->pins_sleep)) {
		ret = pinctrl_select_state(gh2dpk->pinctrl,
					   gh2dpk->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}

	if (!IS_ERR(gh2dpk->power_gpio))
		gpiod_set_value_cansleep(gh2dpk->power_gpio, 0);

	regulator_bulk_disable(GH2DPK_NUM_SUPPLIES, gh2dpk->supplies);
}

static int gh2dpk_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);

	return __gh2dpk_power_on(gh2dpk);
}

static int gh2dpk_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);

	__gh2dpk_power_off(gh2dpk);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gh2dpk_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gh2dpk_mode *def_mode = &supported_modes[0];

	mutex_lock(&gh2dpk->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gh2dpk->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gh2dpk_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops gh2dpk_pm_ops = {
	SET_RUNTIME_PM_OPS(gh2dpk_runtime_suspend,
			   gh2dpk_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gh2dpk_internal_ops = {
	.open = gh2dpk_open,
};
#endif

static const struct v4l2_subdev_core_ops gh2dpk_core_ops = {
	.s_power = gh2dpk_s_power,
	.ioctl = gh2dpk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gh2dpk_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gh2dpk_video_ops = {
	.s_stream = gh2dpk_s_stream,
	.g_frame_interval = gh2dpk_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gh2dpk_pad_ops = {
	.enum_mbus_code = gh2dpk_enum_mbus_code,
	.enum_frame_size = gh2dpk_enum_frame_sizes,
	.enum_frame_interval = gh2dpk_enum_frame_interval,
	.get_fmt = gh2dpk_get_fmt,
	.set_fmt = gh2dpk_set_fmt,
	.get_mbus_config = gh2dpk_g_mbus_config,
};

static const struct v4l2_subdev_ops gh2dpk_subdev_ops = {
	.core	= &gh2dpk_core_ops,
	.video	= &gh2dpk_video_ops,
	.pad	= &gh2dpk_pad_ops,
};

static int gh2dpk_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gh2dpk *gh2dpk = container_of(ctrl->handler,
					     struct gh2dpk, ctrl_handler);
	struct i2c_client *client = gh2dpk->client;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);

		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain value 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set blank value 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "set pattern value 0x%x\n", ctrl->val);
		// ret = gh2dpk_enable_test_pattern(gh2dpk, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const __maybe_unused struct v4l2_ctrl_ops gh2dpk_ctrl_ops = {
	.s_ctrl = gh2dpk_set_ctrl,
};

static int gh2dpk_initialize_controls(struct gh2dpk *gh2dpk)
{
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	int ret;

	handler = &gh2dpk->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &gh2dpk->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	gh2dpk->link_frequency = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
						   0, PIXEL_RATE_WITH_250M_10BIT,
						   1, PIXEL_RATE_WITH_250M_10BIT);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gh2dpk->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gh2dpk->subdev.ctrl_handler = handler;
	gh2dpk->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gh2dpk_configure_regulators(struct gh2dpk *gh2dpk)
{
	unsigned int i;

	for (i = 0; i < GH2DPK_NUM_SUPPLIES; i++)
		gh2dpk->supplies[i].supply = gh2dpk_supply_names[i];

	return devm_regulator_bulk_get(&gh2dpk->client->dev,
				       GH2DPK_NUM_SUPPLIES,
				       gh2dpk->supplies);
}

static void find_terminal_resolution(struct gh2dpk *gh2dpk)
{
	u32 hdr_mode = 0;
	struct device_node *node = gh2dpk->client->dev.of_node;
	int i = 0;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			gh2dpk->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		gh2dpk->cur_mode = &supported_modes[0];
}

static int gh2dpk_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gh2dpk *gh2dpk;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	gh2dpk = devm_kzalloc(dev, sizeof(*gh2dpk), GFP_KERNEL);
	if (!gh2dpk)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gh2dpk->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gh2dpk->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gh2dpk->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gh2dpk->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	gh2dpk->client = client;

	find_terminal_resolution(gh2dpk);

	gh2dpk->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gh2dpk->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gh2dpk->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(gh2dpk->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	gh2dpk->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gh2dpk->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gh2dpk->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gh2dpk->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios, maybe no use\n");

	gh2dpk->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gh2dpk->pinctrl)) {
		gh2dpk->pins_default =
			pinctrl_lookup_state(gh2dpk->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gh2dpk->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gh2dpk->pins_sleep =
			pinctrl_lookup_state(gh2dpk->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gh2dpk->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = gh2dpk_configure_regulators(gh2dpk);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&gh2dpk->mutex);

	sd = &gh2dpk->subdev;
	v4l2_i2c_subdev_init(sd, client, &gh2dpk_subdev_ops);
	ret = gh2dpk_initialize_controls(gh2dpk);
	if (ret)
		goto err_destroy_mutex;

	ret = __gh2dpk_power_on(gh2dpk);
	if (ret)
		goto err_free_handler;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gh2dpk_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gh2dpk->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gh2dpk->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gh2dpk->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gh2dpk->module_index, facing,
		 GH2DPK_NAME, dev_name(sd->dev));
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
	__gh2dpk_power_off(gh2dpk);
err_free_handler:
	v4l2_ctrl_handler_free(&gh2dpk->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gh2dpk->mutex);

	return ret;
}

static void gh2dpk_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gh2dpk *gh2dpk = to_gh2dpk(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gh2dpk->ctrl_handler);
	mutex_destroy(&gh2dpk->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gh2dpk_power_off(gh2dpk);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gh2dpk_of_match[] = {
	{ .compatible = "gh,gh_2dpk" },
	{},
};
MODULE_DEVICE_TABLE(of, gh2dpk_of_match);
#endif

static const struct i2c_device_id gh2dpk_match_id[] = {
	{ "gh,gh_2dpk", 0 },
	{ },
};

static struct i2c_driver gh2dpk_i2c_driver = {
	.driver = {
		.name = GH2DPK_NAME,
		.pm = &gh2dpk_pm_ops,
		.of_match_table = of_match_ptr(gh2dpk_of_match),
	},
	.probe		= gh2dpk_probe,
	.remove		= gh2dpk_remove,
	.id_table	= gh2dpk_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gh2dpk_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gh2dpk_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("gh 2dpk sensor driver");
MODULE_LICENSE("GPL");
