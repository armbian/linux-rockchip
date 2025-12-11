// SPDX-License-Identifier: GPL-2.0
/*
 * gst412c driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 testing .
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
#include <media/v4l2-device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "../platform/rockchip/ooc/rkooc-externel.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#define gst412C_BITS_PER_SAMPLE		14
#define GST412C_PMCLK_FREQ		27000000

#define GST412C_CHIP_ID			0x19
#define GST412C_REG_CHIP_ID		0x30
#define GST412C_REG_INITIAL		0x01

#define GST412C_REG_TEST_PATTERN		0x2f
#define GST412C_TEST_PATTERN_BIT_MASK	BIT(7)

#define REG_DELAY			0xFE
#define REG_NULL			0xFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define gst412C_NAME			"gst412c"

struct regval {
	u8 addr;
	u8 val;
};

struct gst412c_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 bpp;
	const struct regval *reg_list;
};

struct gst412c {
	struct i2c_client *client;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *shutter_p_gpio;
	struct gpio_desc *shutter_m_gpio;
	struct regulator *avdd;
	struct regulator *vdet;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *shutter;
	struct v4l2_ctrl *gst_ra;
	struct v4l2_ctrl *gst_hssd;
	struct v4l2_ctrl *vtemp;
	struct mutex mutex;
	struct v4l2_fract cur_fps;
	bool streaming;
	bool power_on;
	const struct gst412c_mode *cur_mode;
	u32 module_index;
	bool has_init_exp;
	bool is_first_streamoff;
	bool is_standby;

	struct v4l2_subdev *rkooc_sd;

	uint8_t sensor_version;
	//struct v4l2_rect        rect;            /* sensor cropping window */
};

#define to_gst412c(sd) container_of(sd, struct gst412c, subdev)

static const struct regval gst412c_400x300_25fps_12M5_regs[] = {
	{ 0x7c, 0xa9 },
	{ 0x00, 0x00 },
	{ 0x7c, 0xa8 },

	// reg_win_x_size = 400 (0x190)
	{ 0x02, 0x90 },
	{ 0x03, 0x01 },
	// reg_win_y_size = 308 (0x134)
	{ 0x04, 0x34 },
	{ 0x05, 0x01 },
	// reg_x_blank_size = 90 (0x5a)
	{ 0x06, 0x5a },
	{ 0x07, 0x00 },
	// reg_y_blank_size = 243 (0xf3)
	{ 0x08, 0xf3 },
	{ 0x09, 0x00 },
	// reg_y_blank_pixel = 10
	{ 0x0a, 0x0a },
	{ 0x0b, 0x00 },
	// reg_frame_rate = 1/200=50hz
	{ 0x0c, 0xc8 },
	{ 0x0d, 0x00 },

	{ 0x10, 0x01 },

	{ 0x18, 0x3f },		// GS_ST_KEY1
	{ 0x19, 0x2a },		// GS_ST_KEY2
	{ 0x1a, 0x1d },		// GS_ST_KEY3
	{ 0x1b, 0x11 },		// GS_VS_KEY
	{ 0x1c, 0x15 },		// GS_HS_KEY

	// pll config
	{ 0x2f, 0x41 },
	{ 0x30, 0x3c },
	{ 0x31, 0x02 },
	{ 0x32, 0x00 },

	{ 0x01, 0x01 },

	{ REG_NULL, 0x00 },
};

static const struct regval gst412c_common_regs[] = {
	{ 0x7c, 0xab },
	{ 0x01, 0x03 },
	{ 0x05, 0x03 },
	{ 0x06, 0xff },
	{ 0x09, 0x40 },
	{ 0x0a, 0xc0 },
	{ 0x0d, 0x00 },
	{ 0x0f, 0x08 },
	{ 0x10, 0x20 },
	{ 0x11, 0x20 },
	{ 0x19, 0x07 },

	{ 0x1B, 0xFF },
	{ 0x1d, 0x3e },
	{ 0x1e, 0x7f },
	{ 0x20, 0xff },
	{ 0x21, 0x00 },

	{ 0x7c, 0xa9 },
	{ 0x06, 0x37 },
	{ 0x07, 0x10 },
	{ 0x27, 0x0E },

	{ 0x7c, 0xa8 },
	{ 0x20, 0x07 },
	{ 0x24, 0x20 },
	{ 0x25, 0x20 },
	{ 0x1f, 0x0d },
	{ 0x2c, 0x02 },
	{ 0x35, 0x3d },
	{ REG_NULL, 0x00 },
};

static const struct regval gst412c_spot_regs[] = {
	{ 0x7c, 0xa8 },
	{ 0x2D, 0x38 },		//INT_L
	{ 0x2E, 0x00 },		//INT_H
	{ 0x28, 0x05 },		//gain
	{ 0x1f, 0x0d },		//gfid

	{ 0x7c, 0xa9 },
	{ 0x06, 0x37 },
	{ 0x07, 0x12 },
	{ 0x03, 0x01 },
	{ 0x0a, 0x88 },
	{ 0x0b, 0x13 },

	{ 0x7c, 0xab },
	{ 0x18, 0x24 },
	{ 0x12, 0x04 },
	{ 0x19, 0x07 },
	{ 0x06, 0xff },
	{ 0x11, 0x20 },
	{ 0x7c, 0xa8 },

	{ REG_NULL, 0x00 },
};

static const struct gst412c_mode supported_modes[] = {
	{
	 .width = 400,
	 .height = 308,
	 .max_fps = {
		     .numerator = 10000,
		     .denominator = 250000,
		     },
	 .hts_def = 500,
	 .vts_def = 500,
	 .bus_fmt = MEDIA_BUS_FMT_Y14_1X14,
	 .reg_list = gst412c_400x300_25fps_12M5_regs,
	 .bpp = 16,
	  },
};

/* Write registers up to 4 at a time */
static int gst412c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		dev_err(&client->dev,
			"gst412c i2c_master_send failed!, ret %d\n", ret);
		return ret;
	}

	return 0;
}

static int gst412c_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	int ret = 0;
	int i;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gst412c_write_reg(client, regs[i].addr, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int gst412c_read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msgs[2];
	u8 buf[1];
	int ret;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = (u8 *) &reg;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev,
			"gst412c(%x) i2c_transfer failed!, ret %d\n",
			client->addr, ret);
		return -EIO;
	}
	*val = buf[0];

	//dev_info(&client->dev, "gst412c readreg %x %x!\n", reg, buf[0]);
	return 0;
}

static int gst412c_enable_test_pattern(struct gst412c *gst412c, u32 pattern)
{
	u8 val = 0;
	int ret = 0;

	dev_info(&gst412c->client->dev, "%s enable %d\n", __func__, pattern);

	ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xa9);
	ret |=
	    gst412c_read_reg(gst412c->client, GST412C_REG_TEST_PATTERN, &val);
	if (pattern)
		val |= GST412C_TEST_PATTERN_BIT_MASK;
	else
		val &= ~GST412C_TEST_PATTERN_BIT_MASK;

	ret |=
	    gst412c_write_reg(gst412c->client, GST412C_REG_TEST_PATTERN, val);
	ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xa8);

	return ret;
}

static void gst412c_shutter_control(struct gst412c *gst412c, bool on)
{
	if (on) {
		gpiod_set_value(gst412c->shutter_p_gpio, 1);
		gpiod_set_value(gst412c->shutter_m_gpio, 0);
	} else {
		gpiod_set_value(gst412c->shutter_p_gpio, 0);
		gpiod_set_value(gst412c->shutter_m_gpio, 1);
	}

	// reset gpios
	msleep(100);
	gpiod_set_value(gst412c->shutter_p_gpio, 0);
	gpiod_set_value(gst412c->shutter_m_gpio, 0);
}

static int gst412c_set_ra_sel(struct gst412c *gst412c, u8 val)
{
	int ret = 0;
	u8 tmp;

	dev_info(&gst412c->client->dev, "%s value %d\n", __func__, val);

	if (val <= 8) {
		tmp = val;
		tmp = 0xff >> (8 - tmp);
		ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xab);
		ret |= gst412c_write_reg(gst412c->client, 0x20, 0x00);
		ret |= gst412c_write_reg(gst412c->client, 0x21, tmp);
		ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xa8);
	} else if (val <= 16) {
		tmp = val;
		tmp = 0xff >> (16 - tmp);
		ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xab);
		ret |= gst412c_write_reg(gst412c->client, 0x20, tmp);
		ret |= gst412c_write_reg(gst412c->client, 0x21, 0xff);
		ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xa8);
	}

	return ret;
}

static int gst412c_set_hssd(struct gst412c *gst412c, u8 val)
{
	int ret = 0;
	u8 tmp = val & 0x7f;

	dev_info(&gst412c->client->dev, "%s value %d\n", __func__, val);

	ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xab);
	ret |= gst412c_write_reg(gst412c->client, 0x09, tmp);
	ret |= gst412c_write_reg(gst412c->client, 0x7c, 0xa8);

	return ret;
}

static int gst412c_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int gst412c_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct gst412c *gst412c = to_gst412c(sd);
	const struct gst412c_mode *mode = gst412c->cur_mode;

	mutex_lock(&gst412c->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format =
		    *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gst412c->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gst412c->mutex);

	return 0;
}

static int gst412c_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = supported_modes[0].bus_fmt;

	return 0;
}

static int gst412c_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_selection *sel)
{
	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = 400;
		sel->r.top = 4;
		sel->r.height = 300;
		return 0;
	}
	return -EINVAL;
}

static int gst412c_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gst412c_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct gst412c *gst412c = to_gst412c(sd);
	const struct gst412c_mode *mode = gst412c->cur_mode;

	fi->interval = mode->max_fps;
	return 0;
}

static int gst412c_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int gst412c_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				 struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_PARALLEL;
	config->bus.parallel.flags =
	    V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_VSYNC_ACTIVE_HIGH |
	    V4L2_MBUS_PCLK_SAMPLE_RISING;
	return 0;
}

static void gst412c_get_module_inf(struct gst412c *gst412c,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
}

static void gst412c_get_irfpa_info(struct gst412c *gst412c,
				   struct rkmodule_irfpa_info *inf)
{
	inf->irfpa_en = 1;
	inf->gray_dec_en = 0;
	inf->raw14_mode = 1;
}

static long gst412c_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gst412c *gst412c = to_gst412c(sd);
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gst412c_get_module_inf(gst412c, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_IRFPA_INFO:
		gst412c_get_irfpa_info(gst412c,
				       (struct rkmodule_irfpa_info *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gst412c_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rk_sensor_setting *setting;
	long ret;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gst412c_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = kzalloc(sizeof(*setting), GFP_KERNEL);
		if (!setting) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(setting, up, sizeof(*setting));
		if (!ret)
			ret = gst412c_ioctl(sd, cmd, setting);
		else
			ret = -EFAULT;
		kfree(setting);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __gst412c_start_stream(struct gst412c *gst412c)
{
	int ret = 0;
	int cnt = 100;
	bool ready = false;

	// 探测器输出相关寄存器配置，如帧频、窗口、翻转等寄存器配置
	// 数字输入输出接口为单端模式时需要配置场行同步字
	ret = gst412c_write_array(gst412c->client, gst412c->cur_mode->reg_list);
	if (ret) {
		dev_err(&gst412c->client->dev, "gst412c_write_array failed!\n");
		return ret;
	}
	// 等待 300ms
	msleep(300);

	while ((!ready) && (cnt--)) {
		u8 val = 1;

		ret =
		    gst412c_read_reg(gst412c->client, GST412C_REG_INITIAL,
				     &val);
		if (ret)
			break;
		if (val == 0)
			ready = true;
		dev_info(&gst412c->client->dev,
			 "%s waiting for sensor ready...\n", __func__);
		msleep(30);
	}

	if (ret) {
		dev_err(&gst412c->client->dev, "wait sensor ready failed!\n");
		return ret;
	}
	// 帧同步信号输入
	v4l2_subdev_call(gst412c->rkooc_sd, video, s_stream, 1);

	// 调节探测器工作状态
	ret = gst412c_write_array(gst412c->client, gst412c_common_regs);
	if (ret)
		return ret;

	ret = gst412c_write_array(gst412c->client, gst412c_spot_regs);
	if (ret)
		return ret;

	if (gst412c->test_pattern->val)
		gst412c_enable_test_pattern(gst412c, 1);

	gst412c_set_ra_sel(gst412c, gst412c->gst_ra->val);
	gst412c_set_hssd(gst412c, gst412c->gst_hssd->val);
	return ret;
}

static int __gst412c_stop_stream(struct gst412c *gst412c)
{
	int ret = 0;

	v4l2_subdev_call(gst412c->rkooc_sd, video, s_stream, 0);
	return ret;
}

static int __gst412c_power_on(struct gst412c *gst412c);
static int gst412c_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gst412c *gst412c = to_gst412c(sd);
	struct i2c_client *client = gst412c->client;
	int ret = 0;

	mutex_lock(&gst412c->mutex);
	on = !!on;
	if (on == gst412c->streaming)
		goto unlock_and_return;

	if (on) {
		__gst412c_power_on(gst412c);

		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gst412c_start_stream(gst412c);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gst412c_stop_stream(gst412c);
		pm_runtime_put(&client->dev);
	}

	gst412c->streaming = on;

unlock_and_return:
	mutex_unlock(&gst412c->mutex);

	return ret;
}

static int gst412c_s_power(struct v4l2_subdev *sd, int on)
{
	struct gst412c *gst412c = to_gst412c(sd);
	struct i2c_client *client = gst412c->client;
	int ret = 0;

	mutex_lock(&gst412c->mutex);

	/* If the power state is not modified - no work to do. */
	if (gst412c->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gst412c->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gst412c->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gst412c->mutex);

	return ret;
}

static void __gst412c_power_off(struct gst412c *gst412c)
{
	gpiod_set_value(gst412c->reset_gpio, 0);
	msleep(20);
	regulator_disable(gst412c->vdet);
	msleep(20);
	regulator_disable(gst412c->avdd);
	msleep(20);
	v4l2_subdev_call(gst412c->rkooc_sd, video, s_crystal_freq,
			 GST412C_PMCLK_FREQ, 0);
}

static int __gst412c_power_on(struct gst412c *gst412c)
{
	int ret = 0;
	struct device *dev = &gst412c->client->dev;

	if (gst412c->power_on)
		return 0;

	gpiod_set_value(gst412c->reset_gpio, 0);
	msleep(20);

	// 1 外部电源开启 DVDD 上电后 AVDD 上电，最后给 VDET 上电
	ret = regulator_enable(gst412c->avdd);
	if (ret) {
		dev_err(dev, "Failed to enable avdd!\n");
		goto failed;
	}
	msleep(20);

	ret = regulator_enable(gst412c->vdet);
	if (ret) {
		dev_err(dev, "Failed to enable vdet!\n");
		goto failed;
	}
	// 2 等待电源电压稳定
	msleep(20);

	// 3 开启主时钟 给探测器送时钟信号
	ret =
	    v4l2_subdev_call(gst412c->rkooc_sd, video, s_crystal_freq,
			     GST412C_PMCLK_FREQ, 1);
	if (ret < 0)
		dev_warn(dev, "Failed to enable pmclk\n");

	// 4 芯片数字系统初始化 等待 10ms 后 RESETN 置‘1’
	msleep(20);
	gpiod_set_value(gst412c->reset_gpio, 1);

	// 5 等待 10ms
	msleep(20);

	return 0;

failed:
	__gst412c_power_off(gst412c);
	return ret;
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused gst412c_resume(struct device *dev)
{
	return 0;
}

static int __maybe_unused gst412c_suspend(struct device *dev)
{
	return 0;
}
#else
#define gst412c_resume NULL
#define gst412c_suspend NULL
#endif

static int gst412c_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gst412c *gst412c = to_gst412c(sd);

	return __gst412c_power_on(gst412c);
}

static int gst412c_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gst412c *gst412c = to_gst412c(sd);

	__gst412c_power_off(gst412c);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gst412c_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gst412c *gst412c = to_gst412c(sd);
	struct v4l2_mbus_framefmt *try_fmt =
	    v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gst412c_mode *def_mode = &supported_modes[0];

	mutex_lock(&gst412c->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gst412c->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gst412c_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum
				       *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static const struct dev_pm_ops gst412c_pm_ops = {
	SET_RUNTIME_PM_OPS(gst412c_runtime_suspend,
			   gst412c_runtime_resume, NULL)
#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
	    SET_LATE_SYSTEM_SLEEP_PM_OPS(gst412c_suspend, gst412c_resume)
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gst412c_internal_ops = {
	.open = gst412c_open,
};
#endif

static const struct v4l2_subdev_core_ops gst412c_core_ops = {
	.s_power = gst412c_s_power,
	.ioctl = gst412c_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gst412c_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gst412c_video_ops = {
	.s_stream = gst412c_s_stream,
	.g_frame_interval = gst412c_g_frame_interval,
	.s_frame_interval = gst412c_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops gst412c_pad_ops = {
	.enum_mbus_code = gst412c_enum_mbus_code,
	.enum_frame_size = gst412c_enum_frame_sizes,
	.enum_frame_interval = gst412c_enum_frame_interval,
	.get_fmt = gst412c_get_fmt,
	.set_fmt = gst412c_set_fmt,
	.get_selection = gst412c_get_selection,
	//.set_selection = gst412c_set_selection,
	.get_mbus_config = gst412c_g_mbus_config,
};

static const struct v4l2_subdev_ops gst412c_subdev_ops = {
	.core = &gst412c_core_ops,
	.video = &gst412c_video_ops,
	.pad = &gst412c_pad_ops,
};

static int gst412_digital_fpt(struct gst412c *gst412c)
{
	struct i2c_client *client = gst412c->client;
	u8 Dvtemp_d1 = 0, Dvtemp_d2 = 0, Dvtemp_d3 = 0, Dvtemp_d4 =
	    0, Dvtemp_d5 = 0, Dvtemp_d6 = 0, rd_sensor_reg_data =
	    0, wr_data = 0;
	u16 delt_volt = 0, Dvtemp = 0;
	u16 D16_vtemp_d2, D16_vtemp_d1;
	int g17_Vtemp_V, Dvtemp0;

	gst412c_write_reg(client, 0x7c, 0xa8);
	gst412c_write_reg(client, 0x47, 0x01);
	gst412c_read_reg(client, 0x3d, &Dvtemp_d1);
	gst412c_read_reg(client, 0x3e, &Dvtemp_d2);
	gst412c_read_reg(client, 0x3f, &Dvtemp_d3);
	gst412c_read_reg(client, 0x40, &Dvtemp_d4);
	gst412c_read_reg(client, 0x41, &Dvtemp_d5);
	gst412c_read_reg(client, 0x42, &Dvtemp_d6);

	D16_vtemp_d2 = (Dvtemp_d2 << 8) | Dvtemp_d1;
	D16_vtemp_d1 = (Dvtemp_d4 << 8) | Dvtemp_d3;
	Dvtemp = (Dvtemp_d6 << 8) | Dvtemp_d5;

	gst412c_write_reg(client, 0x47, 0x00);
	gst412c_write_reg(client, 0x7c, 0xa9);
	gst412c_read_reg(client, 0x06, &rd_sensor_reg_data);
	wr_data = (rd_sensor_reg_data & 0x30) >> 4;

	if ((gst412c->sensor_version & 0x0f) == 4) {
		switch (wr_data) {
		case 0:
			delt_volt = 250;
			break;
		case 1:
			delt_volt = 350;
			break;
		case 2:
			delt_volt = 550;
			break;
		case 3:
			delt_volt = 750;
			break;
		default:
			break;
		}
	} else {
		delt_volt = 250 + 100 * wr_data;
	}

	gst412c_write_reg(client, 0x7c, 0xa8);

	g17_Vtemp_V =
	    (Dvtemp - D16_vtemp_d1) * 1000 / (D16_vtemp_d2 - D16_vtemp_d1) +
	    delt_volt;
	Dvtemp0 = (g17_Vtemp_V - 1461) * 100 * 100 / 1200 + 30 * 100;

	//dev_info(&client->dev, "enter vtemp g17_Vtemp_V %d, Dvtemp0 %d\n", g17_Vtemp_V, Dvtemp0);
	return Dvtemp0;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev control operations
 */

/*
 * Maximum shutter width used for AEC.
 */
#define V4L2_CID_IRFPA_SHUTTER  (V4L2_CID_USER_BASE | 0x1001)
#define V4L2_CID_IRFPA_GST_RA   (V4L2_CID_USER_BASE | 0x1002)
#define V4L2_CID_IRFPA_GST_HSSD (V4L2_CID_USER_BASE | 0x1003)
#define V4L2_CID_IRFPA_VTEMP    (V4L2_CID_USER_BASE | 0x1004)

static int gst412c_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gst412c *gst412c = container_of(ctrl->handler,
					       struct gst412c, ctrl_handler);

	if (ctrl->id == V4L2_CID_IRFPA_VTEMP) {
		if (gst412c->power_on)
			ctrl->val = gst412_digital_fpt(gst412c);
		else
			ctrl->val = 0;
	}
	return 0;
}

static int gst412c_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gst412c *gst412c = container_of(ctrl->handler,
					       struct gst412c, ctrl_handler);
	struct i2c_client *client = gst412c->client;
	int ret = 0;

	if (ctrl->id == V4L2_CID_IRFPA_SHUTTER) {
		dev_info(&client->dev, "set shutter to %d\n", ctrl->val);
		gst412c_shutter_control(gst412c, ctrl->val);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		dev_info(&client->dev, "set test pattern to %d\n", ctrl->val);
		ret = gst412c_enable_test_pattern(gst412c, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	case V4L2_CID_IRFPA_GST_RA:
		if (gst412c->streaming)
			ret = gst412c_set_ra_sel(gst412c, ctrl->val);
		break;
	case V4L2_CID_IRFPA_GST_HSSD:
		if (gst412c->streaming)
			ret = gst412c_set_hssd(gst412c, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gst412c_ctrl_ops = {
	.s_ctrl = gst412c_set_ctrl,
	.g_volatile_ctrl = gst412c_g_volatile_ctrl
};

static const char *const gst412c_test_pattern_menu[] = {
	"Disabled",
	"Testpattern 1",
};

static const struct v4l2_ctrl_config gst412c_electromagnetic_shutter = {
	.ops = &gst412c_ctrl_ops,
	.id = V4L2_CID_IRFPA_SHUTTER,
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.name = "Shutter",
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = 0,
};

static const struct v4l2_ctrl_config gst412c_gst_rasel = {
	.ops = &gst412c_ctrl_ops,
	.id = V4L2_CID_IRFPA_GST_RA,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "GST sensor RA SEL",
	.min = 0,
	.max = 16,
	.step = 1,
	.def = 7,
	.flags = 0,
};

static const struct v4l2_ctrl_config gst412c_gst_hssd = {
	.ops = &gst412c_ctrl_ops,
	.id = V4L2_CID_IRFPA_GST_HSSD,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "GST sensor HSSD",
	.min = 0,
	.max = 0x7f,
	.step = 1,
	.def = 64,
	.flags = 0,
};

static const struct v4l2_ctrl_config gst412c_vtemp = {
	.ops = &gst412c_ctrl_ops,
	.id = V4L2_CID_IRFPA_VTEMP,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Sensor Vtemp",
	.min = 0,
	.max = 8000,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
};

static int gst412c_initialize_controls(struct gst412c *gst412c)
{
	const struct gst412c_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u32 vblank_def, hblank_def;
	int ret = 0;

	handler = &gst412c->ctrl_handler;
	mode = gst412c->cur_mode;
	v4l2_ctrl_handler_init(handler, 5);
	handler->lock = &gst412c->mutex;

	v4l2_ctrl_new_std(handler, &gst412c_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);

	v4l2_ctrl_new_std(handler, &gst412c_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);

	vblank_def = mode->vts_def - mode->width;
	hblank_def = mode->hts_def - mode->height;
	gst412c->vblank = v4l2_ctrl_new_std(handler, &gst412c_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    vblank_def, 1, vblank_def);
	gst412c->hblank =
	    v4l2_ctrl_new_std(handler, &gst412c_ctrl_ops, V4L2_CID_HBLANK,
			      hblank_def, hblank_def, 1, hblank_def);
	gst412c->vblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	gst412c->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	gst412c->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							     &gst412c_ctrl_ops,
							     V4L2_CID_TEST_PATTERN,
							     ARRAY_SIZE
							     (gst412c_test_pattern_menu)
							     - 1, 0, 0,
							     gst412c_test_pattern_menu);

	gst412c->shutter =
	    v4l2_ctrl_new_custom(handler, &gst412c_electromagnetic_shutter,
				 NULL);
	gst412c->gst_ra =
	    v4l2_ctrl_new_custom(handler, &gst412c_gst_rasel, NULL);
	gst412c->gst_hssd =
	    v4l2_ctrl_new_custom(handler, &gst412c_gst_hssd, NULL);
	gst412c->vtemp = v4l2_ctrl_new_custom(handler, &gst412c_vtemp, NULL);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gst412c->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gst412c->subdev.ctrl_handler = handler;
	gst412c->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gst412c_check_sensor_id(struct gst412c *gst412c,
				   struct i2c_client *client)
{
	struct device *dev = &gst412c->client->dev;
	int ret;
	u8 id = 0;
	u8 ver = 0;

	gst412c_write_reg(client, 0x7c, 0xa8);

	ret = gst412c_read_reg(client, GST412C_REG_CHIP_ID, &id);
	if (id != GST412C_CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	gst412c_write_reg(client, 0x7c, 0xa9);
	gst412c_read_reg(client, 0x32, &ver);
	gst412c_write_reg(client, 0x7c, 0xa9);

	gst412c->sensor_version = ver;
	dev_info(dev, "Detected gst412C (%06x), version %x\n",
		 GST412C_REG_CHIP_ID, ver);
	return 0;
}

static int gst412c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gst412c *gst412c;
	struct v4l2_subdev *sd;
	int ret = 0;
	struct device_node *rkooc_node = NULL;
	struct platform_device *rkooc_pdev = NULL;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	gst412c = devm_kzalloc(dev, sizeof(*gst412c), GFP_KERNEL);
	if (!gst412c)
		return -ENOMEM;

	gst412c->client = client;
	gst412c->cur_mode = &supported_modes[0];

	gst412c->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gst412c->reset_gpio)) {
		dev_err(dev, "Failed to get reset_gpio\n");
		return -EINVAL;
	}

	gst412c->shutter_p_gpio =
	    devm_gpiod_get(dev, "shutter_p", GPIOD_OUT_LOW);
	if (IS_ERR(gst412c->shutter_p_gpio)) {
		dev_err(dev, "Failed to get shutter_p_gpio\n");
		return -EINVAL;
	}

	gst412c->shutter_m_gpio =
	    devm_gpiod_get(dev, "shutter_m", GPIOD_OUT_LOW);
	if (IS_ERR(gst412c->shutter_m_gpio)) {
		dev_err(dev, "Failed to get shutter_m_gpio\n");
		return -EINVAL;
	}

	gst412c->vdet = devm_regulator_get(dev, "irfpa_vdet");
	if (IS_ERR_OR_NULL(gst412c->vdet)) {
		dev_err(dev, "Failed to get regulator vdet\n");
		return -EINVAL;
	}

	gst412c->avdd = devm_regulator_get(dev, "irfpa_avdd");
	if (IS_ERR_OR_NULL(gst412c->avdd)) {
		dev_err(dev, "Failed to get regulator avdd\n");
		return -EINVAL;
	}

	rkooc_node = of_parse_phandle(node, "rkooc", 0);
	if (rkooc_node) {
		rkooc_pdev = of_find_device_by_node(rkooc_node);
		of_node_put(rkooc_node);
		if (rkooc_pdev)
			gst412c->rkooc_sd = platform_get_drvdata(rkooc_pdev);
	}
	if (IS_ERR_OR_NULL(gst412c->rkooc_sd)) {
		dev_err(dev, "get rkooc sub device failed!\n");
		ret = -EINVAL;
		goto err_power_off;
	}

	v4l2_subdev_call(gst412c->rkooc_sd, core, ioctl,
			 RKOOC_CMD_CONFIG_SENSOR, (void *)RKOOC_SENSOR_GST412C);

	mutex_init(&gst412c->mutex);

	sd = &gst412c->subdev;
	v4l2_i2c_subdev_init(sd, client, &gst412c_subdev_ops);
	ret = gst412c_initialize_controls(gst412c);
	if (ret)
		goto err_destroy_mutex;

	ret = __gst412c_power_on(gst412c);
	if (ret)
		goto err_free_handler;

	ret = gst412c_check_sensor_id(gst412c, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gst412c_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gst412c->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gst412c->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	snprintf(sd->name, sizeof(sd->name), "m%02d_f_%s %s",
		 gst412c->module_index, gst412C_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	dev_info(&client->dev, "gst412c probe done, enter pm_runtime_idle.\n");
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__gst412c_power_off(gst412c);
err_free_handler:
	v4l2_ctrl_handler_free(&gst412c->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gst412c->mutex);

	return ret;
}

static void gst412c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gst412c *gst412c = to_gst412c(sd);

	dev_info(&client->dev, "gst412c remove!\n");

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gst412c->ctrl_handler);
	mutex_destroy(&gst412c->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gst412c_power_off(gst412c);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gst412c_of_match[] = {
	{.compatible = "globalsensor,gst412c" },
	{ },
};

MODULE_DEVICE_TABLE(of, gst412c_of_match);
#endif

static const struct i2c_device_id gst412c_match_id[] = {
	{ "globalsensor,gst412c", 0 },
	{ },
};

static struct i2c_driver gst412c_i2c_driver = {
	.driver = {
		   .name = gst412C_NAME,
		   .pm = &gst412c_pm_ops,
		   .of_match_table = of_match_ptr(gst412c_of_match),
		    },
	.probe = &gst412c_probe,
	.remove = &gst412c_remove,
	.id_table = gst412c_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gst412c_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gst412c_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("gst412c sensor driver");
MODULE_LICENSE("GPL");
