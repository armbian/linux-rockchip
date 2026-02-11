// SPDX-License-Identifier: GPL-2.0
/* aw.c   aw codec driver
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 *  Author: Bruce zhao <zhaolei@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <sound/tlv.h>
#include <linux/uaccess.h>

#include "aw.h"
#include "aw_bin_parse.h"
#include "aw_device.h"
#include "aw_log.h"
#include "aw_spin.h"

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW_I2C_NAME "aw_smartpa"

#define AW_DRIVER_VERSION "v1.6.0"

#define AW_RATES   (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_96000)
#define AW_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
		    SNDRV_PCM_FMTBIT_S32_LE)

#define AW_ACF_FILE      "aw881xx_acf.bin"
#define AW_REQUEST_FW_RETRIES 5 /* 5 times */

static unsigned int g_aw_dev_cnt;
static DEFINE_MUTEX(g_aw_lock);
static struct aw_container *g_awinic_cfg;

static const char *const aw_switch[] = {"Disable", "Enable"};

int aw_get_version(char *buf, int size)
{
	if (size > strlen(AW_DRIVER_VERSION)) {
		memcpy(buf, AW_DRIVER_VERSION, strlen(AW_DRIVER_VERSION));
		return strlen(AW_DRIVER_VERSION);
	} else {
		return -ENOMEM;
	}
}

/******************************************************
 *
 * aw append suffix sound channel information
 *
 ******************************************************/
static void *aw_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = NULL;

	str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);
	if (!str)
		return str;

	memcpy(str, buf, strlen(buf));
	return str;
}

static int aw_append_i2c_suffix(char *format, const char **change_name,
				struct aw *aw)
{
	char buf[64] = {0};
	int i2cbus = aw->i2c->adapter->nr;
	int addr = aw->i2c->addr;

	snprintf(buf, sizeof(buf), format, *change_name, i2cbus, addr);
	*change_name = aw_devm_kstrdup(aw->dev, buf);
	if (!(*change_name))
		return -ENOMEM;

	aw_dev_info(aw->dev, "change name :%s", *change_name);
	return 0;
}

static int aw_append_channel_suffix(char *format, const char **change_name,
				    struct aw *aw)
{
	char buf[64] = {0};
	int channel = aw->aw_pa->channel;

	snprintf(buf, sizeof(buf), format, *change_name, channel);
	*change_name = aw_devm_kstrdup(aw->dev, buf);
	if (!(*change_name))
		return -ENOMEM;

	aw_dev_info(aw->dev, "change name :%s", *change_name);
	return 0;
}

/******************************************************
 *
 * aw distinguish between codecs and components by version
 *
 ******************************************************/
#ifdef AW_KERNEL_VER_OVER_4_19_1
static struct aw_componet_codec_ops aw_componet_codec_ops = {
	.kcontrol_codec = snd_soc_kcontrol_component,
	.codec_get_drvdata = snd_soc_component_get_drvdata,
	.add_codec_controls = snd_soc_add_component_controls,
	.unregister_codec = snd_soc_unregister_component,
	.register_codec = snd_soc_register_component,
};
#else
static struct aw_componet_codec_ops aw_componet_codec_ops = {
	.kcontrol_codec = snd_soc_kcontrol_codec,
	.codec_get_drvdata = snd_soc_codec_get_drvdata,
	.add_codec_controls = snd_soc_add_codec_controls,
	.unregister_codec = snd_soc_unregister_codec,
	.register_codec = snd_soc_register_codec,
};
#endif

static aw_snd_soc_codec_t *aw_get_codec(struct snd_soc_dai *dai)
{
#ifdef AW_KERNEL_VER_OVER_4_19_1
	return dai->component;
#else
	return dai->codec;
#endif
}

/******************************************************
 *
 * aw reg write/read
 *
 ******************************************************/

static int aw_i2c_writes(struct aw *aw, u8 reg_addr, u8 *buf, u16 len)
{
	int ret = -1;
	u8 *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw->i2c, data, len + 1);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c master send error");
		goto i2c_master_error;
	} else if (ret != (len + 1)) {
		aw_dev_err(aw->dev, "i2c master send error(size error)");
		ret = -ENXIO;
		goto i2c_master_error;
	}

	kfree(data);
	return 0;

i2c_master_error:
	kfree(data);
	return ret;
}

static int aw_i2c_reads(struct aw *aw, u8 reg_addr, u8 *data_buf, u16 data_len)
{
	int ret = -1;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw->i2c->addr,
			.flags = 0,
			.len = sizeof(u8),
			.buf = &reg_addr,
		},
		[1] = {
			.addr = aw->i2c->addr,
			.flags = I2C_M_RD,
			.len = data_len,
			.buf = data_buf,
		},
	};

	ret = i2c_transfer(aw->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c_transfer failed");
		return ret;
	} else if (ret != AW_READ_MSG_NUM) {
		aw_dev_err(aw->dev, "transfer failed(size error)");
		return -ENXIO;
	}

	return 0;
}

static int aw_i2c_write(struct aw *aw, u8 reg_addr, u16 reg_data)
{
	int ret = -1;
	u8 cnt = 0;
	u8 buf[2] = {0};

	buf[0] = (reg_data & 0xff00) >> 8;
	buf[1] = (reg_data & 0x00ff) >> 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = aw_i2c_writes(aw, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw->dev, "i2c_write cnt=%d error=%d",
				   cnt, ret);
		} else {
			break;
		}
		cnt++;
	}

	if (aw->i2c_log_en)
		aw_dev_info(aw->dev, "write: reg = 0x%02x, val = 0x%04x",
			    reg_addr, reg_data);

	return ret;
}

static int aw_i2c_read(struct aw *aw, u8 reg_addr, u16 *reg_data)
{
	int ret = -1;
	u8 cnt = 0;
	u8 buf[2] = {0};

	while (cnt < AW_I2C_RETRIES) {
		ret = aw_i2c_reads(aw, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw->dev, "i2c_read cnt=%d error=%d",
				   cnt, ret);
		} else {
			*reg_data = (buf[0] << 8) | (buf[1] << 0);
			break;
		}
		cnt++;
	}

	if (aw->i2c_log_en)
		aw_dev_info(aw->dev, "read: reg = 0x%02x, val = 0x%04x",
			    reg_addr, *reg_data);

	return ret;
}

static int aw_i2c_write_bits(struct aw *aw, u8 reg_addr, u16 mask, u16 reg_data)
{
	int ret = -1;
	u16 reg_val = 0;

	ret = aw_i2c_read(aw, reg_addr, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data & (~mask);
	ret = aw_i2c_write(aw, reg_addr, reg_val);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c read error, ret=%d", ret);
		return ret;
	}

	return 0;
}

int aw_reg_write(struct aw *aw, u8 reg_addr, u16 reg_data)
{
	int ret = -1;

	mutex_lock(&aw->i2c_lock);
	ret = aw_i2c_write(aw, reg_addr, reg_data);
	if (ret < 0)
		aw_dev_err(aw->dev,
			   "write fail, reg = 0x%02x, val = 0x%04x, ret=%d",
			   reg_addr, reg_data, ret);
	mutex_unlock(&aw->i2c_lock);

	return ret;
}

int aw_reg_read(struct aw *aw, u8 reg_addr, u16 *reg_data)
{
	int ret = -1;

	mutex_lock(&aw->i2c_lock);
	ret = aw_i2c_read(aw, reg_addr, reg_data);
	if (ret < 0)
		aw_dev_err(aw->dev,
			   "read fail: reg = 0x%02x, val = 0x%04x, ret=%d",
			   reg_addr, *reg_data, ret);
	mutex_unlock(&aw->i2c_lock);

	return ret;
}

int aw_reg_write_bits(struct aw *aw, u8 reg_addr, u16 mask, u16 reg_data)
{
	int ret = -1;

	mutex_lock(&aw->i2c_lock);
	ret = aw_i2c_write_bits(aw, reg_addr, mask, reg_data);
	if (ret < 0)
		aw_dev_err(aw->dev, "write fail, ret=%d", ret);
	mutex_unlock(&aw->i2c_lock);

	return ret;
}

static int aw_dsp_write_16bit(struct aw *aw, u16 dsp_addr, u32 dsp_data)
{
	int ret = -1;
	struct aw_dsp_mem_desc *desc = &aw->aw_pa->dsp_mem_desc;

	ret = aw_i2c_write(aw, desc->dsp_madd_reg, dsp_addr);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	ret = aw_i2c_write(aw, desc->dsp_mdat_reg, (u16)dsp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

static int aw_dsp_write_32bit(struct aw *aw, u16 dsp_addr, u32 dsp_data)
{
	int ret = -1;
	u16 temp_data = 0;
	struct aw_dsp_mem_desc *desc = &aw->aw_pa->dsp_mem_desc;

	ret = aw_i2c_write(aw, desc->dsp_madd_reg, dsp_addr);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	temp_data = dsp_data & AW_DSP_16_DATA_MASK;
	ret = aw_i2c_write(aw, desc->dsp_mdat_reg, temp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	temp_data = dsp_data >> 16;
	ret = aw_i2c_write(aw, desc->dsp_mdat_reg, temp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

/******************************************************
 * aw clear dsp chip select state
 ******************************************************/
static void aw_clear_dsp_sel_st(struct aw *aw)
{
	u16 reg_value = 0;
	u8 reg = aw->aw_pa->soft_rst.reg;

	aw_i2c_read(aw, reg, &reg_value);
}

int aw_dsp_write(struct aw *aw, u16 dsp_addr, u32 dsp_data, u8 data_type)
{
	int ret = -1;

	mutex_lock(&aw->i2c_lock);
	if (data_type == AW_DSP_16_DATA) {
		ret = aw_dsp_write_16bit(aw, dsp_addr, dsp_data);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "write dsp_addr[0x%04x] 16 bit dsp_data[%04x] failed",
				   (u32)dsp_addr, dsp_data);
			goto exit;
		}
	} else if (data_type == AW_DSP_32_DATA) {
		ret = aw_dsp_write_32bit(aw, dsp_addr, dsp_data);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "write dsp_addr[0x%04x] 32 bit dsp_data[%08x] failed",
				   (u32)dsp_addr, dsp_data);
			goto exit;
		}
	} else {
		aw_dev_err(aw->dev, "data type[%d] unsupported", data_type);
		ret = -EINVAL;
		goto exit;
	}

exit:
	aw_clear_dsp_sel_st(aw);
	mutex_unlock(&aw->i2c_lock);
	return ret;
}

int aw_dsp_writes(struct aw *aw, u8 *data, u32 len, u16 base)
{
	int i = 0;
	struct aw_dsp_mem_desc *desc = &aw->aw_pa->dsp_mem_desc;

#ifdef AW_DSP_I2C_WRITES
	u32 tmp_len = 0;
	u32 data_len = 0;
#else
	u16 reg_val = 0;
#endif

	mutex_lock(&aw->i2c_lock);
#ifdef AW_DSP_I2C_WRITES
	/* i2c writes */
	aw_i2c_write(aw, desc->dsp_madd_reg, base);

	for (i = 0; i < len; i += AW_MAX_RAM_WRITE_BYTE_SIZE) {
		data_len = (u32)(len - i);
		if (data_len < AW_MAX_RAM_WRITE_BYTE_SIZE)
			tmp_len = data_len;
		else
			tmp_len = AW_MAX_RAM_WRITE_BYTE_SIZE;
		aw_i2c_writes(aw, desc->dsp_mdat_reg, &data[i], tmp_len);
	}
#else
	/* i2c write */
	aw_i2c_write(aw, desc->dsp_madd_reg, base);
	for (i = 0; i < len; i += 2) {
		reg_val = (data[i] << 8) + data[i + 1];
		aw_i2c_write(aw, desc->dsp_mdat_reg, reg_val);
	}
#endif
	mutex_unlock(&aw->i2c_lock);

	return 0;
}

static int aw_dsp_read_16bit(struct aw *aw, u16 dsp_addr, u32 *dsp_data)
{
	int ret = -1;
	u16 temp_data = 0;
	struct aw_dsp_mem_desc *desc = &aw->aw_pa->dsp_mem_desc;

	ret = aw_i2c_write(aw, desc->dsp_madd_reg, dsp_addr);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	ret = aw_i2c_read(aw, desc->dsp_mdat_reg, &temp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	*dsp_data = temp_data;

	return 0;
}

static int aw_dsp_read_32bit(struct aw *aw, u16 dsp_addr, u32 *dsp_data)
{
	int ret = -1;
	u16 temp_data = 0;
	struct aw_dsp_mem_desc *desc = &aw->aw_pa->dsp_mem_desc;

	/*write dsp addr*/
	ret = aw_i2c_write(aw, desc->dsp_madd_reg, dsp_addr);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	/*get Low 16 bit data*/
	ret = aw_i2c_read(aw, desc->dsp_mdat_reg, &temp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c read error, ret=%d", ret);
		return ret;
	}

	*dsp_data = temp_data;

	/*get high 16 bit data*/
	ret = aw_i2c_read(aw, desc->dsp_mdat_reg, &temp_data);
	if (ret < 0) {
		aw_dev_err(aw->dev, "i2c read error, ret=%d", ret);
		return ret;
	}
	*dsp_data |= (temp_data << 16);

	return 0;
}

int aw_dsp_read(struct aw *aw, u16 dsp_addr, u32 *dsp_data, u8 data_type)
{
	int ret = -1;

	mutex_lock(&aw->i2c_lock);
	if (data_type == AW_DSP_16_DATA) {
		ret = aw_dsp_read_16bit(aw, dsp_addr, dsp_data);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "read dsp_addr[0x%04x] 16 bit dsp_data[%04x] failed",
				   (u32)dsp_addr, *dsp_data);
			goto exit;
		}
	} else if (data_type == AW_DSP_32_DATA) {
		ret = aw_dsp_read_32bit(aw, dsp_addr, dsp_data);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "read dsp_addr[0x%04x] 32 bit dsp_data[%08x] failed",
				   (u32)dsp_addr, *dsp_data);
			goto exit;
		}
	} else {
		aw_dev_err(aw->dev, "data type[%d] unsupported", data_type);
		ret = -EINVAL;
		goto exit;
	}

exit:
	aw_clear_dsp_sel_st(aw);
	mutex_unlock(&aw->i2c_lock);
	return ret;
}

int aw_dsp_write_bits(struct aw *aw, u16 dsp_addr, u32 dsp_mask,
		      u32 dsp_data, u8 data_type)
{
	int ret = -1;
	u32 read_val = 0;
	u32 write_val = 0;

	mutex_lock(&aw->i2c_lock);
	if (data_type == AW_DSP_16_DATA) {
		ret = aw_dsp_read_16bit(aw, dsp_addr, &read_val);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "read dsp_addr[0x%04x] 16 bit dsp_data[%04x] failed",
				   (u32)dsp_addr, read_val);
			goto exit;
		}
	} else if (data_type == AW_DSP_32_DATA) {
		ret = aw_dsp_read_32bit(aw, dsp_addr, &read_val);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "read dsp_addr[0x%04x] 32 bit dsp_data[%08x] failed",
				   (u32)dsp_addr, read_val);
			goto exit;
		}
	}

	write_val = read_val & dsp_mask;
	write_val |= dsp_data & (~dsp_mask);

	if (data_type == AW_DSP_16_DATA) {
		ret = aw_dsp_write_16bit(aw, dsp_addr, write_val);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "write dsp_addr[0x%04x] 16 bit dsp_data[%04x] failed",
				   (u32)dsp_addr, write_val);
			goto exit;
		}
	} else if (data_type == AW_DSP_32_DATA) {
		ret = aw_dsp_write_32bit(aw, dsp_addr, write_val);
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "write dsp_addr[0x%04x] 32 bit dsp_data[%08x] failed",
				   (u32)dsp_addr, write_val);
			goto exit;
		}
	}
exit:
	aw_clear_dsp_sel_st(aw);
	mutex_unlock(&aw->i2c_lock);
	return ret;
}

/******************************************************
 * aw get dev num
 ******************************************************/
int aw_get_dev_num(void)
{
	return g_aw_dev_cnt;
}

/******************************************************
 *
 * Digital Audio Interface
 *
 ******************************************************/
static int aw_startup(struct snd_pcm_substream *substream,
		      struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	int ret = -1;
	u32 re = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw_dev_info(aw->dev, "playback enter");
		/*load cali re*/
		if (aw->aw_pa->cali_desc.cali_re == AW_ERRO_CALI_RE_VALUE) {
			ret = aw_device_params(aw->aw_pa, AW_DEV_BIN_RE_PARAMS,
					       (void *)&re, sizeof(re),
					       AW_GET_DEV_PARAMS);
			if (ret < 0)
				aw_dev_err(aw->dev, "get cali re failed");
			else
				aw->aw_pa->cali_desc.cali_re = re;
		}
	} else {
		aw_dev_info(aw->dev, "capture enter");
	}

	return 0;
}

static int aw_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);

	aw_dev_info(codec->dev, "fmt=0x%x", fmt);

	return 0;
}

static int aw_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
			     unsigned int freq, int dir)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_info(aw->dev, "freq=%d", freq);

	aw->sysclk = freq;
	return 0;
}

static int aw_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	/* get CAPTURE rate param  bit width*/
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		aw_dev_info(aw->dev,
			    "STREAM_CAPTURE requested rate: %d, width = %d",
			    params_rate(params), params_width(params));

	/* get PLAYBACK rate param  bit width*/
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aw_dev_info(aw->dev,
			    "STREAM_PLAYBACK requested rate: %d, width = %d",
			    params_rate(params), params_width(params));

	return 0;
}

static void aw_start_pa(struct aw *aw)
{
	int ret = -1;
	int i = 0;

	aw_dev_info(aw->dev, "enter");

	if (aw->allow_pw == false) {
		aw_dev_info(aw->dev, "dev can not allow power");
		return;
	}

	if (aw->pstream == AW_STREAM_CLOSE) {
		aw_dev_info(aw->dev, "pstream is close");
		return;
	}

	for (i = 0; i < AW_START_RETRIES; i++) {
		ret = aw_device_start(aw->aw_pa);
		if (ret) {
			aw_dev_err(aw->dev, "start failed");
			ret = aw_device_fw_update(aw->aw_pa,
						  AW_DSP_FW_UPDATE_ON, true);
			if (ret < 0) {
				aw_dev_err(aw->dev, "fw update failed");
				continue;
			}
		} else {
			aw_dev_info(aw->dev, "start success");
			break;
		}
	}
}

static void aw_startup_work(struct work_struct *work)
{
	struct aw *aw = container_of(work, struct aw, start_work.work);

	mutex_lock(&aw->lock);
	aw_start_pa(aw);
	mutex_unlock(&aw->lock);
}

static void aw_start(struct aw *aw, bool sync_start)
{
	int ret = -1;
	int i = 0;

	if (aw->aw_pa->fw_status == AW_DEV_FW_OK) {
		if (aw->allow_pw == false) {
			aw_dev_info(aw->dev, "dev can not allow power");
			return;
		}

		if (aw->aw_pa->status == AW_DEV_PW_ON)
			return;

		for (i = 0; i < AW_START_RETRIES; i++) {
			ret = aw_device_fw_update(aw->aw_pa,
						  AW_DSP_FW_UPDATE_OFF,
						  aw->phase_sync);
			if (ret < 0) {
				aw_dev_err(aw->dev, "fw update failed");
				continue;
			} else {
				/*firmware update success*/
				if (sync_start == AW_SYNC_START)
					aw_start_pa(aw);
				else
					queue_delayed_work(aw->work_queue,
							   &aw->start_work,
							   AW_START_WORK_DELAY_MS);
				return;
			}
		}
	}
}

static int aw_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	u32 spin_params = AW_DSP_SPIN_ST;

	aw_dev_info(aw->dev, "mute state=%d", mute);

	if (stream != SNDRV_PCM_STREAM_PLAYBACK) {
		aw_dev_info(aw->dev, "capture");
		return 0;
	}

	if (mute) {
		aw->pstream = AW_STREAM_CLOSE;
		cancel_delayed_work_sync(&aw->start_work);
		mutex_lock(&aw->lock);
		aw_device_stop(aw->aw_pa);
		mutex_unlock(&aw->lock);
	} else {
		aw->pstream = AW_STREAM_OPEN;
		mutex_lock(&aw->lock);
		aw_start(aw, AW_ASYNC_START);
		aw_device_params(aw->aw_pa, AW_DEV_HOLD_SPIN_PARAMS,
				 (void *)&spin_params, sizeof(spin_params),
				 AW_SET_DEV_PARAMS);
		mutex_unlock(&aw->lock);
	}

	return 0;
}

static void aw_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aw_dev_info(aw->dev, "stream playback");
	else
		aw_dev_info(aw->dev, "stream capture");
}

static const struct snd_soc_dai_ops aw_dai_ops = {
	.startup = aw_startup,
	.set_fmt = aw_set_fmt,
	.set_sysclk = aw_set_dai_sysclk,
	.hw_params = aw_hw_params,
	.mute_stream = aw_mute,
	.shutdown = aw_shutdown,
};

static struct snd_soc_dai_driver aw_dai[] = {
	{
		.name = "aw-aif",
		.id = 1,
		.playback = {
			.stream_name = "Speaker_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AW_RATES,
			.formats = AW_FORMATS,
		},
#if 0
	.capture = {
		.stream_name = "Speaker_Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AW_RATES,
		.formats = AW_FORMATS,
		},
#endif
		.ops = &aw_dai_ops,
		/*.symmetric_rates = 1,*/
	},
};

static int aw_dai_drv_append_suffix(struct aw *aw,
				     struct snd_soc_dai_driver *dai_drv,
				     int num_dai)
{
	int ret = -1;
	int i = 0;

	if ((dai_drv != NULL) && (num_dai > 0)) {
		for (i = 0; i < num_dai; i++) {
			if (aw->rename_flag == AW_RENAME_ENABLE) {
				ret = aw_append_channel_suffix("%s-%d",
							       &dai_drv->name,
							       aw);
				if (ret < 0)
					return ret;
				ret = aw_append_channel_suffix("%s_%d",
						&dai_drv->playback.stream_name,
						aw);
				if (ret < 0)
					return ret;
				ret = aw_append_channel_suffix("%s_%d",
						&dai_drv->capture.stream_name,
						aw);
				if (ret < 0)
					return ret;
				dev_set_name(aw->dev, "%s_%d", "aw_smartpa",
					     aw->aw_pa->channel);
				aw_dev_info(aw->dev, "change dev_name:%s",
					    dev_name(aw->dev));

			} else {
				ret = aw_append_i2c_suffix("%s-%d-%x",
							   &dai_drv->name, aw);
				if (ret < 0)
					return ret;
				ret = aw_append_i2c_suffix("%s_%d_%x",
						&dai_drv->playback.stream_name,
						aw);
				if (ret < 0)
					return ret;
				ret = aw_append_i2c_suffix("%s_%d_%x",
						&dai_drv->capture.stream_name,
						aw);
				if (ret < 0)
					return ret;
			}

			aw_dev_info(aw->dev, "dai name [%s]", dai_drv[i].name);
			aw_dev_info(aw->dev, "pstream_name [%s]",
				    dai_drv[i].playback.stream_name);
			aw_dev_info(aw->dev, "cstream_name [%s]",
				    dai_drv[i].capture.stream_name);
		}
	}

	return 0;
}

/******************************************************
 * aw interrupt
 ******************************************************/

static void aw_interrupt_work(struct work_struct *work)
{
	int ret = -1;
	u32 sys_status = 0;
	u32 crc_switch_status = 0;
	u32 crc_check_abnormal = 0;

	u32 mask_value = AW_DEV_UNMASK_INT_VAL;
	struct aw *aw = container_of(work, struct aw, interrupt_work.work);
	struct aw_device *aw_dev = aw->aw_pa;

	aw_dev_info(aw->dev, "enter");

	mutex_lock(&aw->lock);

	/*read sysst value*/
	ret = aw_device_params(aw_dev, AW_DEV_SYSST_PARAMS, (void *)&sys_status,
			       sizeof(sys_status), AW_GET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw->dev, "get sysst value failed");
	} else {
		aw_dev_info(aw->dev, "sysst value 0x%x", sys_status);
	}

	ret = aw_device_params(aw_dev, AW_DEV_REALTIME_CRC_GET_PARAMS,
			       (void *)&crc_switch_status,
			       sizeof(crc_switch_status), AW_GET_DEV_PARAMS);
	if (ret < 0)
		aw_dev_err(aw->dev, "get crc_status failed");

	if (crc_switch_status) {
		ret = aw_crc_realtime_check(aw_dev, &crc_check_abnormal);
		if (ret < 0)
			aw_dev_err(aw->dev, "crc_realtime_check failed");

		if (crc_check_abnormal) {
			/*pa stop or stopping just set profile*/
			if (aw->pstream) {
				aw_device_stop(aw->aw_pa);

				ret = aw_device_fw_update(aw_dev,
							  AW_DSP_FW_UPDATE_ON,
							  true);
				if (ret < 0)
					aw_dev_err(aw->dev, "fw update failed");

				aw_start(aw, AW_SYNC_START);
			}
		}
	}

	/*read and clear int reg*/
	aw_device_status(aw_dev, AW_DEV_CLEAR_INT_STATUS, AW_SET_DEV_STATUS);

	/*unmask interrupt*/
	ret = aw_device_params(aw_dev, AW_DEV_INT_PARAMS, (void *)&mask_value,
			       sizeof(mask_value), AW_SET_DEV_PARAMS);
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "write_params failed");

	mutex_unlock(&aw->lock);
}

/*****************************************************
 *
 * codec driver
 *
 *****************************************************/
static int aw_get_fade_in_time(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	u32 time = 0;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw->aw_pa;

	ret = aw_device_params(aw_dev, AW_DEV_FADE_IN_TIME_PARAMS,
			       (void *)&time, sizeof(time), AW_GET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read_params failed");
		return ret;
	}
	ucontrol->value.integer.value[0] = time;

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);

	return 0;
}

static int aw_set_fade_in_time(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	u32 time = 0;
	int ret = -1;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw->aw_pa;

	if ((ucontrol->value.integer.value[0] > mc->max) ||
	    (ucontrol->value.integer.value[0] < mc->min)) {
		aw_pr_dbg("set val %ld overflow %d or less than :%d",
			  ucontrol->value.integer.value[0], mc->max, mc->min);
		return 0;
	}
	time = ucontrol->value.integer.value[0];

	ret = aw_device_params(aw_dev, AW_DEV_FADE_IN_TIME_PARAMS,
			       (void *)&time, sizeof(time), AW_SET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write_params failed");
		return ret;
	}

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);
	return 0;
}

static int aw_get_fade_out_time(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	u32 time = 0;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw->aw_pa;

	ret = aw_device_params(aw_dev, AW_DEV_FADE_OUT_TIME_PARAMS,
			       (void *)&time, sizeof(time),
			       AW_GET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read_params failed");
		return ret;
	}

	ucontrol->value.integer.value[0] = time;

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);

	return 0;
}

static int aw_set_fade_out_time(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u32 time = 0;
	int ret = -1;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw->aw_pa;

	if ((ucontrol->value.integer.value[0] > mc->max) ||
	    (ucontrol->value.integer.value[0] < mc->min)) {
		aw_pr_dbg("set val %ld overflow %d or less than :%d",
			  ucontrol->value.integer.value[0], mc->max, mc->min);
		return 0;
	}

	time = ucontrol->value.integer.value[0];

	ret = aw_device_params(aw_dev, AW_DEV_FADE_OUT_TIME_PARAMS,
			       (void *)&time, sizeof(time),
			       AW_SET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write_params failed");
		return ret;
	}

	aw_pr_dbg("step time %d", time);

	return 0;
}

static struct snd_kcontrol_new aw_controls[] = {
	SOC_SINGLE_EXT("aw_fadein_us", 0, 0, 1000000, 0,
		       aw_get_fade_in_time, aw_set_fade_in_time),
	SOC_SINGLE_EXT("aw_fadeout_us", 0, 0, 1000000, 0,
		       aw_get_fade_out_time, aw_set_fade_out_time),
};

static void aw_add_codec_controls(struct aw *aw)
{
	aw_dev_info(aw->dev, "enter");

	if (aw->aw_pa->channel == 0) {
		aw_componet_codec_ops.add_codec_controls(aw->codec,
							  &aw_controls[0],
							  ARRAY_SIZE(aw_controls));
		aw_add_spin_controls((void *)aw);
	}
}

static int aw_profile_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	int count = 0;
	char *name = NULL;
	const char *prof_name = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	int ret = 0;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;

	count = aw_dev_get_profile_count(aw->aw_pa);
	if (count <= 0) {
		uinfo->value.enumerated.items = 0;
		aw_pr_err("get count[%d] failed", count);
		return 0;
	}

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	name = uinfo->value.enumerated.name;
	count = uinfo->value.enumerated.item;

	prof_name = aw_dev_get_prof_name(aw->aw_pa, count);
	if (prof_name == NULL) {
		ret = strscpy(uinfo->value.enumerated.name, "null",
			      strlen("null") + 1);
		if (ret < 0)
			aw_pr_err("copy profile name [null] failed");
		return 0;
	}

	ret = strscpy(name, prof_name, sizeof(uinfo->value.enumerated.name));
	if (ret < 0)
		aw_pr_err("copy profile name failed");

	return 0;
}

static int aw_profile_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw_dev_get_profile_index(aw->aw_pa);
	aw_dev_dbg(codec->dev, "profile index [%d]",
		   aw_dev_get_profile_index(aw->aw_pa));
	return 0;
}

static int aw_profile_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	int ret = -1;
	int cur_index = 0;

	if (aw->dbg_en_prof == false) {
		aw_dev_info(codec->dev, "profile close");
		return 0;
	}

	/* check value valid */
	ret = aw_dev_check_profile_index(aw->aw_pa,
					 ucontrol->value.integer.value[0]);
	if (ret) {
		aw_dev_info(codec->dev, "unsupported index %ld",
			    ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	/*check cur_index == set value*/
	cur_index = aw_dev_get_profile_index(aw->aw_pa);
	if (cur_index == ucontrol->value.integer.value[0]) {
		aw_dev_info(codec->dev, "index no change");
		return 0;
	}

	/*pa stop or stopping just set profile*/
	mutex_lock(&aw->lock);
	aw_dev_set_profile_index(aw->aw_pa, ucontrol->value.integer.value[0]);

	if (aw->pstream) {
		aw_device_stop(aw->aw_pa);
		aw_start(aw, AW_SYNC_START);
	}

	mutex_unlock(&aw->lock);

	aw_dev_info(codec->dev, "profile id %ld",
		    ucontrol->value.integer.value[0]);
	return 1;
}

static int aw_switch_info(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_info *uinfo)
{
	int count = 0;
	int ret = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	count = 2;

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	ret = strscpy(uinfo->value.enumerated.name,
		      aw_switch[uinfo->value.enumerated.item],
		      strlen(aw_switch[uinfo->value.enumerated.item]) + 1);
	if (ret < 0)
		aw_pr_err("copy switch name failed");

	return 0;
}

static int aw_switch_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw->allow_pw;

	return 0;
}

static int aw_switch_set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_info(codec->dev, "set value:%ld",
		    ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] == aw->allow_pw) {
		aw_dev_info(aw->dev, "PA switch not change");
		return 0;
	}

	if (aw->pstream) {
		if (ucontrol->value.integer.value[0] == 0) {
			cancel_delayed_work_sync(&aw->start_work);
			mutex_lock(&aw->lock);
			aw_device_stop(aw->aw_pa);
			aw->allow_pw = false;
			mutex_unlock(&aw->lock);
		} else {
			cancel_delayed_work_sync(&aw->start_work);
			mutex_lock(&aw->lock);
			aw->allow_pw = true;
			aw_start(aw, AW_SYNC_START);
			mutex_unlock(&aw->lock);
		}
	} else {
		mutex_lock(&aw->lock);
		if (ucontrol->value.integer.value[0])
			aw->allow_pw = true;
		else
			aw->allow_pw = false;
		mutex_unlock(&aw->lock);
	}

	return 1;
}

static int aw_monitor_switch_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	int count = 0;
	int ret = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	count = 2;

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	ret = strscpy(uinfo->value.enumerated.name,
		      aw_switch[uinfo->value.enumerated.item],
		      strlen(aw_switch[uinfo->value.enumerated.item]) + 1);
	if (ret < 0)
		aw_pr_err("copy monitor switch name failed");

	return 0;
}

static int aw_monitor_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_monitor_desc *monitor_desc = &aw->aw_pa->monitor_desc;

	ucontrol->value.integer.value[0] =
		monitor_desc->monitor_cfg.monitor_switch;

	return 0;
}

static int aw_monitor_switch_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_monitor_desc *monitor_desc = &aw->aw_pa->monitor_desc;
	u32 enable = 0;

	aw_dev_info(codec->dev, "set monitor_switch:%ld",
		    ucontrol->value.integer.value[0]);

	enable = ucontrol->value.integer.value[0];

	if (monitor_desc->monitor_cfg.monitor_switch == enable) {
		aw_dev_info(aw->dev, "monitor_switch not change");
		return 0;
	}
	monitor_desc->monitor_cfg.monitor_switch = enable;
	if (enable)
		aw_monitor_start(monitor_desc);

	return 1;
}

static int aw_volume_info(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_info *uinfo)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	/* set kcontrol info */
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = vol_desc->mute_volume;

	return 0;
}

static int aw_volume_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	ucontrol->value.integer.value[0] = vol_desc->ctl_volume;

	aw_dev_info(aw->dev, "ucontrol->value.integer.value[0]=%d",
		    vol_desc->ctl_volume);

	return 0;
}

static int aw_volume_set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	u16 value = 0;
	u32 compared_vol = 0;
	int ret = -1;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw->aw_pa;
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	value = ucontrol->value.integer.value[0];
	if (value > vol_desc->mute_volume) {
		aw_dev_err(aw->dev, "value over range");
		return -EINVAL;
	}

	aw_dev_info(aw->dev, "ucontrol->value.integer.value[0]=%d", value);

	if (vol_desc->ctl_volume == value)
		return 0;

	vol_desc->ctl_volume = value;

	/*get smaller dB*/
	compared_vol = AW_GET_MAX_VALUE(vol_desc->ctl_volume,
					vol_desc->monitor_volume);

	ret = aw_device_params(aw_dev, AW_DEV_VOLUME_PARAMS,
			       (void *)&compared_vol, sizeof(compared_vol),
			       AW_SET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write_params failed");
		return -EINVAL;
	}

	return 1;
}

static int aw_dynamic_create_controls(struct aw *aw)
{
	struct snd_kcontrol_new *aw_dev_control = NULL;
	char *kctl_name = NULL;

	aw_dev_control = devm_kzalloc(aw->codec->dev,
				       sizeof(struct snd_kcontrol_new) *
				       AW_KCONTROL_NUM, GFP_KERNEL);
	if (!aw_dev_control)
		return -ENOMEM;

	kctl_name = devm_kzalloc(aw->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%u_prof",
		 aw->aw_pa->channel);

	aw_dev_control[0].name = kctl_name;
	aw_dev_control[0].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw_dev_control[0].info = aw_profile_info;
	aw_dev_control[0].get = aw_profile_get;
	aw_dev_control[0].put = aw_profile_set;

	kctl_name = devm_kzalloc(aw->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%u_switch",
		 aw->aw_pa->channel);

	aw_dev_control[1].name = kctl_name;
	aw_dev_control[1].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw_dev_control[1].info = aw_switch_info;
	aw_dev_control[1].get = aw_switch_get;
	aw_dev_control[1].put = aw_switch_set;

	kctl_name = devm_kzalloc(aw->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%u_monitor_switch",
		 aw->aw_pa->channel);

	aw_dev_control[2].name = kctl_name;
	aw_dev_control[2].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw_dev_control[2].info = aw_monitor_switch_info;
	aw_dev_control[2].get = aw_monitor_switch_get;
	aw_dev_control[2].put = aw_monitor_switch_set;

	kctl_name = devm_kzalloc(aw->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%u_rx_volume",
		 aw->aw_pa->channel);

	aw_dev_control[3].name = kctl_name;
	aw_dev_control[3].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw_dev_control[3].info = aw_volume_info;
	aw_dev_control[3].get = aw_volume_get;
	aw_dev_control[3].put = aw_volume_set;

	aw_componet_codec_ops.add_codec_controls(aw->codec, aw_dev_control,
						  AW_KCONTROL_NUM);

	return 0;
}

static int aw_request_firmware_file(struct aw *aw)
{
	const struct firmware *cont = NULL;
	struct aw_container *aw_cfg = NULL;
	int ret = -1;
	int i = 0;
	bool ret_st = false;

	aw->aw_pa->fw_status = AW_DEV_FW_FAILED;

	for (i = 0; i < AW_REQUEST_FW_RETRIES; i++) {
		ret = request_firmware(&cont, AW_ACF_FILE, aw->dev);
		if ((ret < 0) || (!cont)) {
			aw->fw_retry_cnt++;
			aw_dev_err(aw->dev, "load [%s] try [%d]!",
				   AW_ACF_FILE, aw->fw_retry_cnt);

			if (aw->fw_retry_cnt == AW_REQUEST_FW_RETRIES) {
				aw->fw_retry_cnt = 0;
				return ret;
			}
			msleep(1000);
		} else {
			break;
		}
	}

	aw_dev_info(aw->dev, "loaded %s - size: %zu",
		    AW_ACF_FILE, cont ? cont->size : 0);

	mutex_lock(&g_aw_lock);
	if (g_awinic_cfg == NULL) {
		aw_cfg = vzalloc(cont->size + sizeof(int));
		if (aw_cfg == NULL) {
			release_firmware(cont);
			mutex_unlock(&g_aw_lock);
			return -ENOMEM;
		}
		aw_cfg->len = (int)cont->size;
		memcpy(aw_cfg->data, cont->data, cont->size);
		ret = aw_dev_load_acf_check(aw_cfg);
		if (ret < 0) {
			aw_dev_err(aw->dev, "Load [%s] failed ....!",
				   AW_ACF_FILE);
			vfree(aw_cfg);
			aw_cfg = NULL;
			release_firmware(cont);
			mutex_unlock(&g_aw_lock);
			return ret;
		}
		g_awinic_cfg = aw_cfg;
	} else {
		aw_cfg = g_awinic_cfg;
		aw_dev_info(aw->dev, "[%s] already loaded...", AW_ACF_FILE);
	}
	release_firmware(cont);
	mutex_unlock(&g_aw_lock);

	mutex_lock(&aw->lock);
	/*aw device init*/
	ret = aw_device_init(aw->aw_pa, aw_cfg);
	if (ret < 0) {
		aw_dev_info(aw->dev, "dev init failed");
		mutex_unlock(&aw->lock);
		return ret;
	}

	ret = aw_dynamic_create_controls(aw);
	if (ret < 0)
		aw_dev_err(aw->dev, "create control failed");

	ret_st = aw_device_status(aw->aw_pa, AW_DEV_CHECK_SPIN_MODE_STATUS,
				  AW_SET_DEV_STATUS);
	if (!ret_st)
		aw_dev_err(aw->dev, "set spin status failed");

	mutex_unlock(&aw->lock);

	return 0;
}

static void aw_fw_work(struct work_struct *work)
{
	struct aw *aw = container_of(work, struct aw, acf_work.work);
	int ret = -1;

	ret = aw_request_firmware_file(aw);
	if (ret < 0)
		aw_dev_err(aw->dev, "load profile failed");
}

static void aw_load_fw(struct aw *aw)
{
	int ret = -1;

	if (aw->sync_load) {
		/*sync loading*/
		ret = aw_request_firmware_file(aw);
		if (ret < 0)
			aw_dev_err(aw->dev, "load profile failed");
	} else {
		/*async loading*/
		queue_delayed_work(aw->work_queue, &aw->acf_work,
				   msecs_to_jiffies(AW_LOAD_FW_DELAY_TIME));
	}
}

#ifdef AW_MTK_PLATFORM

static const struct snd_soc_dapm_widget aw_dapm_widgets[] = {
	/* playback */
	SND_SOC_DAPM_AIF_IN("AIF_RX", "Speaker_Playback", 0, SND_SOC_NOPM,
			    0, 0),
	SND_SOC_DAPM_OUTPUT("audio_out"),
	/* capture */
	SND_SOC_DAPM_AIF_OUT("AIF_TX", "Speaker_Capture", 0, SND_SOC_NOPM,
			     0, 0),
	SND_SOC_DAPM_INPUT("iv_in"),
};

static const struct snd_soc_dapm_route aw_audio_map[] = {
	{"audio_out", NULL, "AIF_RX"},
	{"AIF_TX", NULL, "iv_in"},
};

#if KERNEL_VERSION(4, 2, 0) > LINUX_VERSION_CODE
static struct snd_soc_dapm_context *snd_soc_codec_get_dapm(struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif

static int aw_add_widgets(struct aw *aw)
{
	int i = 0;
	int ret = -1;
	struct snd_soc_dapm_widget *aw_widgets = NULL;
	struct snd_soc_dapm_route *aw_route = NULL;
#ifdef AW_KERNEL_VER_OVER_4_19_1
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(aw->codec);
#else
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(aw->codec);
#endif

	/*add widgets*/
	aw_widgets = devm_kzalloc(aw->dev,
				   sizeof(struct snd_soc_dapm_widget) *
				   ARRAY_SIZE(aw_dapm_widgets), GFP_KERNEL);
	if (!aw_widgets)
		return -ENOMEM;

	memcpy(aw_widgets, aw_dapm_widgets,
	       sizeof(struct snd_soc_dapm_widget) *
	       ARRAY_SIZE(aw_dapm_widgets));

	for (i = 0; i < ARRAY_SIZE(aw_dapm_widgets); i++) {
		if (aw->rename_flag == AW_RENAME_ENABLE) {
			if (aw_widgets[i].name) {
				ret = aw_append_channel_suffix("%s_%d",
							       &aw_widgets[i].name,
							       aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_widgets.name append channel suffix failed!\n");
					return ret;
				}
			}

			if (aw_widgets[i].sname) {
				ret = aw_append_channel_suffix("%s_%d",
							       &aw_widgets[i].sname,
							       aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_widgets.name append channel suffix failed!");
					return ret;
				}
			}
		} else {
			if (aw_widgets[i].name) {
				ret = aw_append_i2c_suffix("%s_%d_%x",
							   &aw_widgets[i].name,
							   aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_widgets.name append i2c suffix failed!\n");
					return ret;
				}
			}

			if (aw_widgets[i].sname) {
				ret = aw_append_i2c_suffix("%s_%d_%x",
							   &aw_widgets[i].sname,
							   aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_widgets.name append i2c suffix failed!");
					return ret;
				}
			}
		}
	}

	snd_soc_dapm_new_controls(dapm, aw_widgets, ARRAY_SIZE(aw_dapm_widgets));

	/*add route*/
	aw_route = devm_kzalloc(aw->dev,
				 sizeof(struct snd_soc_dapm_route) *
				 ARRAY_SIZE(aw_audio_map), GFP_KERNEL);
	if (!aw_route)
		return -ENOMEM;

	memcpy(aw_route, aw_audio_map,
	       sizeof(struct snd_soc_dapm_route) *
	       ARRAY_SIZE(aw_audio_map));

	for (i = 0; i < ARRAY_SIZE(aw_audio_map); i++) {
		if (aw->rename_flag == AW_RENAME_ENABLE) {
			if (aw_route[i].sink) {
				ret = aw_append_channel_suffix("%s_%d",
							       &aw_route[i].sink,
							       aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_route.sink append channel suffix failed!");
					return ret;
				}
			}
			if (aw_route[i].source) {
				ret = aw_append_channel_suffix("%s_%d",
							       &aw_route[i].source,
							       aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_route.source append channel suffix failed!");
					return ret;
				}
			}
		} else {
			if (aw_route[i].sink) {
				ret = aw_append_i2c_suffix("%s_%d_%x",
							   &aw_route[i].sink,
							   aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_route.sink append i2c suffix failed!");
					return ret;
				}
			}
			if (aw_route[i].source) {
				ret = aw_append_i2c_suffix("%s_%d_%x",
							   &aw_route[i].source,
							   aw);
				if (ret < 0) {
					aw_dev_err(aw->dev,
						   "aw_route.source append i2c suffix failed!");
					return ret;
				}
			}
		}
	}
	snd_soc_dapm_add_routes(dapm, aw_route, ARRAY_SIZE(aw_audio_map));

	return 0;
}
#endif

static int aw_codec_probe(aw_snd_soc_codec_t *aw_codec)
{
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(aw_codec);

	aw_dev_info(aw->dev, "enter");

	/*destroy_workqueue(struct workqueue_struct *wq)*/
	aw->work_queue = create_singlethread_workqueue("aw");
	if (!aw->work_queue) {
		aw_dev_err(aw->dev, "create workqueue failed !");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&aw->interrupt_work, aw_interrupt_work);
	INIT_DELAYED_WORK(&aw->start_work, aw_startup_work);
	INIT_DELAYED_WORK(&aw->acf_work, aw_fw_work);

	aw->codec = aw_codec;

	aw_add_codec_controls(aw);
#ifdef AW_MTK_PLATFORM
	aw_add_widgets(aw);
#endif
	aw_load_fw(aw);

	return 0;
}

#ifdef AW_KERNEL_VER_OVER_4_19_1
static void aw_codec_remove(aw_snd_soc_codec_t *aw_codec)
{
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(aw_codec);

	aw_dev_info(aw->dev, "enter");

	cancel_delayed_work_sync(&aw->interrupt_work);
	cancel_delayed_work_sync(&aw->acf_work);
	cancel_delayed_work_sync(&aw->aw_pa->monitor_desc.delay_work);
	cancel_delayed_work_sync(&aw->start_work);

	if (aw->work_queue)
		destroy_workqueue(aw->work_queue);

	aw_device_deinit(aw->aw_pa);
}
#else
static int aw_codec_remove(aw_snd_soc_codec_t *aw_codec)
{
	struct aw *aw = aw_componet_codec_ops.codec_get_drvdata(aw_codec);

	aw_dev_info(aw->dev, "enter");

	cancel_delayed_work_sync(&aw->interrupt_work);
	cancel_delayed_work_sync(&aw->acf_work);
	cancel_delayed_work_sync(&aw->aw_pa->monitor_desc.delay_work);
	cancel_delayed_work_sync(&aw->start_work);

	if (aw->work_queue)
		destroy_workqueue(aw->work_queue);

	aw_device_deinit(aw->aw_pa);

	return 0;
}
#endif

#ifdef AW_KERNEL_VER_OVER_4_19_1
static const struct snd_soc_component_driver soc_codec_dev_aw = {
	.probe = aw_codec_probe,
	.remove = aw_codec_remove,
};
#else
static const struct snd_soc_codec_driver soc_codec_dev_aw = {
	.probe = aw_codec_probe,
	.remove = aw_codec_remove,
};
#endif

static int aw_componet_codec_register(struct aw *aw)
{
	struct snd_soc_dai_driver *dai_drv = NULL;
	int ret = -1;

	dai_drv = devm_kzalloc(aw->dev, sizeof(aw_dai), GFP_KERNEL);
	if (!dai_drv)
		return -ENOMEM;

	memcpy(dai_drv, aw_dai, sizeof(aw_dai));

	ret = aw_dai_drv_append_suffix(aw, dai_drv, 1);
	if (ret < 0)
		return ret;

	ret = aw->codec_ops->register_codec(aw->dev, &soc_codec_dev_aw, dai_drv,
					    ARRAY_SIZE(aw_dai));
	if (ret < 0) {
		aw_dev_err(aw->dev, "failed to register aw: %d", ret);
		return ret;
	}

	if (aw->rename_flag == AW_RENAME_ENABLE) {
		dev_set_name(aw->dev, "%d-00%x", aw->i2c->adapter->nr,
			     aw->i2c->addr);
		aw_dev_info(aw->dev, "reset dev_name:%s", dev_name(aw->dev));
	}

	return 0;
}

static struct aw *aw_malloc_init(struct i2c_client *i2c)
{
	struct aw *aw = devm_kzalloc(&i2c->dev, sizeof(struct aw), GFP_KERNEL);

	if (!aw)
		return NULL;

	aw->dev = &i2c->dev;
	aw->i2c = i2c;
	aw->aw_pa = NULL;
	aw->codec = NULL;
	aw->codec_ops = &aw_componet_codec_ops;
	aw->dbg_en_prof = true;
	aw->allow_pw = true;
	aw->work_queue = NULL;
	aw->i2c_log_en = 0;
	mutex_init(&aw->lock);
	mutex_init(&aw->i2c_lock);

	return aw;
}

static int aw_gpio_request(struct aw *aw)
{
	int ret = -1;

	if (gpio_is_valid(aw->reset_gpio)) {
		ret = devm_gpio_request_one(aw->dev, aw->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "aw_rst");
		if (ret) {
			aw_dev_err(aw->dev, "rst request failed");
			return ret;
		}
	}

	if (gpio_is_valid(aw->irq_gpio)) {
		ret = devm_gpio_request_one(aw->dev, aw->irq_gpio, GPIOF_IN,
					    "aw_int");
		if (ret) {
			aw_dev_err(aw->dev, "int request failed");
			return ret;
		}
	}

	return 0;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw_parse_gpio_dt(struct aw *aw)
{
	struct device_node *np = aw->dev->of_node;

	aw->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw->reset_gpio < 0) {
		aw_dev_err(aw->dev,
			   "no reset gpio provided, will not hw reset");
		/* return -EIO; */
	} else {
		aw_dev_info(aw->dev, "reset gpio provided ok");
	}

	aw->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw->irq_gpio < 0)
		aw_dev_info(aw->dev, "no irq gpio provided.");
	else
		aw_dev_info(aw->dev, "irq gpio provided ok.");

	return 0;
}

static void aw_parse_sync_flag_dt(struct aw *aw)
{
	int ret = -1;
	int32_t sync_enable = 0;
	struct device_node *np = aw->dev->of_node;

	ret = of_property_read_u32(np, "sync-flag", &sync_enable);
	if (ret < 0) {
		aw_dev_info(aw->dev,
			    "read sync flag failed,default phase sync off");
		sync_enable = false;
	} else {
		aw_dev_info(aw->dev, "sync flag is %d", sync_enable);
	}

	aw->phase_sync = sync_enable;
}

static void aw_parse_rename_flag_dt(struct aw *aw)
{
	int ret = -1;
	u32 rename_enable = 0;
	struct device_node *np = aw->dev->of_node;

	ret = of_property_read_u32(np, "rename-flag", &rename_enable);
	if (ret < 0) {
		aw_dev_info(aw->dev,
			    "read rename flag failed,default rename off");
	} else {
		aw_dev_info(aw->dev, "rename flag is %d", rename_enable);
	}

	aw->rename_flag = rename_enable;
}

static void aw_parse_sync_load_dt(struct aw *aw)
{
	int ret = -1;
	int32_t sync_load = 0;
	struct device_node *np = aw->dev->of_node;

	ret = of_property_read_u32(np, "sync-load", &sync_load);
	if (ret < 0) {
		aw_dev_info(aw->dev,
			    "read sync load failed,default async loading fw");
		sync_load = false;
	} else {
		aw_dev_info(aw->dev, "sync load is %d", sync_load);
	}

	aw->sync_load = sync_load;
}

static int aw_parse_dt(struct aw *aw)
{
	aw_parse_sync_flag_dt(aw);
	aw_parse_rename_flag_dt(aw);
	aw_parse_sync_load_dt(aw);
	return aw_parse_gpio_dt(aw);
}

static int aw_hw_reset(struct aw *aw)
{
	aw_dev_info(aw->dev, "enter");

	if (gpio_is_valid(aw->reset_gpio)) {
		gpio_set_value_cansleep(aw->reset_gpio, 0);
		usleep_range(AW_1000_US, AW_1000_US + 10);
		gpio_set_value_cansleep(aw->reset_gpio, 1);
		usleep_range(AW_1000_US, AW_1000_US + 10);
	} else {
		aw_dev_err(aw->dev, "failed");
		/* return -EINVAL; */
	}
	return 0;
}

static int aw_match_chipid(struct aw *aw)
{
	int ret = -1;
	u16 reg_val = 0;

	ret = aw_reg_read(aw, AW_CHIP_ID_REG, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw->dev, "failed to read chip id, ret=%d", ret);
		return -EIO;
	}

	aw_dev_info(aw->dev, "read chip id: 0x%x", reg_val);

	ret = aw_init_check_chipid(aw, reg_val);
	if (ret < 0)
		return ret;

	return 0;
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
static irqreturn_t aw_irq(int irq, void *data)
{
	int ret = -1;
	struct aw *aw = data;
	struct aw_device *aw_dev = aw->aw_pa;
	u32 mask_value = AW_DEV_MASK_INT_VAL;

	if (!aw) {
		aw_pr_err("pointer is NULL");
		return -EINVAL;
	}

	aw_dev_info(aw->dev, "enter");

	/*mask all irq*/
	ret = aw_device_params(aw_dev, AW_DEV_INT_PARAMS, (void *)&mask_value,
			       sizeof(mask_value), AW_SET_DEV_PARAMS);
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "write_params failed");

	/*upload workqueue*/
	if (aw->work_queue)
		queue_delayed_work(aw->work_queue, &aw->interrupt_work, 0);

	return IRQ_HANDLED;
}

static int aw_interrupt_init(struct aw *aw)
{
	int irq_flags = 0;
	int ret = -1;

	if (gpio_is_valid(aw->irq_gpio)) {
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(aw->dev,
						gpio_to_irq(aw->irq_gpio),
						NULL, aw_irq, irq_flags,
						"aw", aw);
		if (ret) {
			aw_dev_err(aw->dev,
				   "Failed to request IRQ %d: %d",
				   gpio_to_irq(aw->irq_gpio), ret);
			return ret;
		}
	} else {
		aw_dev_info(aw->dev, "skipping IRQ registration");
	}

	return 0;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t reg_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	int reg_num = aw_dev->ops.aw_get_reg_num();
	ssize_t len = 0;
	int i = 0;
	u16 reg_val = 0;

	for (i = 0; i < reg_num; i++) {
		if (aw_dev->ops.aw_check_rd_access(i)) {
			aw_dev->ops.aw_reg_read(aw_dev, i, &reg_val);
			len += snprintf(buf + len, PAGE_SIZE - len,
					"reg:0x%02x=0x%04x\n", i, reg_val);
		}
	}

	return len;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw_dev->ops.aw_reg_write(aw_dev, databuf[0], databuf[1]);

	return count;
}

static ssize_t rw_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw->reg_addr = (u8)databuf[0];
		if (aw_dev->ops.aw_check_rd_access(databuf[0]))
			aw_dev->ops.aw_reg_write(aw_dev, databuf[0],
						 databuf[1]);
	} else if (kstrtouint(buf, 16, &databuf[0]) == 0) {
		aw->reg_addr = (u8)databuf[0];
	}

	return count;
}

static ssize_t rw_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	ssize_t len = 0;
	u16 reg_val = 0;

	if (aw_dev->ops.aw_check_rd_access(aw->reg_addr)) {
		aw_dev->ops.aw_reg_read(aw_dev, aw->reg_addr, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%04x\n", aw->reg_addr, reg_val);
	}

	return len;
}

static ssize_t drv_ver_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "driver_ver: %s\n",
			AW_DRIVER_VERSION);

	return len;
}

static ssize_t dsp_rw_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;
	u16 reg_val = 0;

	mutex_lock(&aw->i2c_lock);
	aw_i2c_write(aw, aw->aw_pa->dsp_mem_desc.dsp_madd_reg, aw->dsp_addr);
	aw_i2c_read(aw, aw->aw_pa->dsp_mem_desc.dsp_mdat_reg, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len, "dsp:0x%04x=0x%04x\n",
			aw->dsp_addr, reg_val);
	aw_i2c_read(aw, aw->aw_pa->dsp_mem_desc.dsp_mdat_reg, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len, "dsp:0x%04x=0x%04x\n",
			aw->dsp_addr + 1, reg_val);
	aw_clear_dsp_sel_st(aw);
	mutex_unlock(&aw->i2c_lock);

	return len;
}

static ssize_t dsp_rw_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	unsigned int databuf[2] = {0};
	u32 crc_status = AW_DSP_CRC_BYPASS;

	aw_device_params(aw->aw_pa, AW_DEV_CRC_FLAG_PARAMS,
			 (void *)&crc_status, sizeof(crc_status),
			 AW_SET_DEV_PARAMS);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw->dsp_addr = (unsigned int)databuf[0];
		aw_dev->ops.aw_dsp_write(aw_dev, databuf[0], databuf[1],
					 AW_DSP_16_DATA);
		aw_dev_dbg(aw->dev, "get param: %x %x",
			   databuf[0], databuf[1]);
	} else if (kstrtouint(buf, 16, &databuf[0]) == 0) {
		aw->dsp_addr = (unsigned int)databuf[0];
		aw_dev_dbg(aw->dev, "get param: %x", databuf[0]);
	}
	aw_clear_dsp_sel_st(aw);

	return count;
}

static int aw_awrw_write(struct aw *aw, const char *buf, size_t count)
{
	int i = 0;
	int ret = -1;
	char *data_buf = NULL;
	int str_len = 0;
	int data_len = 0;
	int temp_data = 0;
	struct aw_i2c_packet *packet = &aw->i2c_packet;
	u32 dsp_addr_h = 0, dsp_addr_l = 0;
	u32 crc_status = AW_DSP_CRC_BYPASS;

	if (!buf) {
		aw_dev_err(aw->dev, "awrw buf is NULL");
		return -EINVAL;
	}

	data_len = AWRW_DATA_BYTES * packet->reg_num;

	str_len = count - AWRW_HDR_LEN - 1;
	if ((data_len * 5 - 1) > str_len) {
		aw_dev_err(aw->dev,
			   "data_str_len [%d], requeset len [%d]",
			   str_len, (data_len * 5 - 1));
		return -EINVAL;
	}

	if (packet->reg_addr == aw->aw_pa->dsp_mem_desc.dsp_madd_reg) {
		if (sscanf(buf + AWRW_HDR_LEN + 1, "0x%02x 0x%02x",
			   &dsp_addr_h, &dsp_addr_l) == 2) {
			packet->dsp_addr = (dsp_addr_h << 8) | dsp_addr_l;
			packet->dsp_status = AWRW_DSP_READY;

			aw_device_params(aw->aw_pa, AW_DEV_CRC_FLAG_PARAMS,
					 (void *)&crc_status,
					 sizeof(crc_status),
					 AW_SET_DEV_PARAMS);

			aw_dev_dbg(aw->dev,
				   "write:reg_addr[0x%02x], dsp_base_addr:[0x%02x]",
				   packet->reg_addr, packet->dsp_addr);
			return 0;
		}
		aw_dev_err(aw->dev, "get reg 0x%x data failed",
			   packet->reg_addr);
		return -EINVAL;
	}

	mutex_lock(&aw->i2c_lock);
	if (packet->reg_addr == aw->aw_pa->dsp_mem_desc.dsp_mdat_reg) {
		if (packet->dsp_status != AWRW_DSP_READY) {
			aw_dev_err(aw->dev, "please write reg[0x40] first");
			ret = -EINVAL;
			goto exit;
		}
		aw_i2c_write(aw, aw->aw_pa->dsp_mem_desc.dsp_madd_reg,
			     packet->dsp_addr);
		packet->dsp_status = AWRW_DSP_ST_NONE;
	}

	aw_dev_info(aw->dev, "write:reg_addr[0x%02x], reg_num[%d]",
		    packet->reg_addr, packet->reg_num);

	data_buf = devm_kzalloc(aw->dev, data_len, GFP_KERNEL);
	if (!data_buf) {
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < data_len; i++) {
		ret = sscanf(buf + AWRW_HDR_LEN + 1 + i * 5, "0x%02x",
			     &temp_data);
		if (ret <= 0) {
			aw_pr_err("sscanf buf failed");
			return -EINVAL;
		}
		data_buf[i] = temp_data;
	}

	ret = aw_i2c_writes(aw, packet->reg_addr, data_buf, data_len);
	if (ret < 0) {
		aw_dev_err(aw->dev, "write failed");
		devm_kfree(aw->dev, data_buf);
		data_buf = NULL;
		goto exit;
	}

	devm_kfree(aw->dev, data_buf);
	data_buf = NULL;
	aw_dev_info(aw->dev, "write success");
exit:
	mutex_unlock(&aw->i2c_lock);
	return ret;
}

static int aw_awrw_data_check(struct aw *aw, int *data)
{
	if ((data[AWRW_HDR_ADDR_BYTES] != AWRW_ADDR_BYTES) ||
	    (data[AWRW_HDR_DATA_BYTES] != AWRW_DATA_BYTES)) {
		aw_dev_err(aw->dev,
			   "addr_bytes [%d] or data_bytes [%d] unsupport",
			   data[AWRW_HDR_ADDR_BYTES],
			   data[AWRW_HDR_DATA_BYTES]);
		return -EINVAL;
	}

	return 0;
}

/* flag addr_bytes data_bytes reg_num reg_addr*/
static int aw_awrw_parse_buf(struct aw *aw, const char *buf, size_t count)
{
	int data[AWRW_HDR_MAX] = {0};
	struct aw_i2c_packet *packet = &aw->i2c_packet;
	int ret = -1;

	if (sscanf(buf, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		   &data[AWRW_HDR_WR_FLAG], &data[AWRW_HDR_ADDR_BYTES],
		   &data[AWRW_HDR_DATA_BYTES], &data[AWRW_HDR_REG_NUM],
		   &data[AWRW_HDR_REG_ADDR]) == 5) {

		ret = aw_awrw_data_check(aw, data);
		if (ret < 0)
			return ret;

		packet->reg_addr = data[AWRW_HDR_REG_ADDR];
		packet->reg_num = data[AWRW_HDR_REG_NUM];

		if (data[AWRW_HDR_WR_FLAG] == AWRW_FLAG_WRITE) {
			return aw_awrw_write(aw, buf, count);
		} else if (data[AWRW_HDR_WR_FLAG] == AWRW_FLAG_READ) {
			packet->i2c_status = AWRW_I2C_ST_READ;
			aw_dev_info(aw->dev,
				    "read_cmd:reg_addr[0x%02x], reg_num[%d]",
				    packet->reg_addr, packet->reg_num);

		} else {
			aw_dev_err(aw->dev,
				   "please check str format, unsupport flag %d",
				   data[AWRW_HDR_WR_FLAG]);
			return -EINVAL;
		}
	} else {
		aw_dev_err(aw->dev, "can not parse string");
		return -EINVAL;
	}

	return 0;
}

static ssize_t awrw_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	int ret = -1;

	if (count < AWRW_HDR_LEN) {
		aw_dev_err(dev,
			   "data count too smaller, please check write format");
		aw_dev_err(dev, "string %s", buf);
		return -EINVAL;
	}

	ret = aw_awrw_parse_buf(aw, buf, count);
	if (ret)
		return -EINVAL;

	return count;
}

static ssize_t awrw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_i2c_packet *packet = &aw->i2c_packet;
	int data_len, len = 0;
	int ret = -1;
	int i = 0;
	char *reg_data = NULL;

	if (packet->i2c_status != AWRW_I2C_ST_READ) {
		aw_dev_err(aw->dev, "please write read cmd first");
		return -EINVAL;
	}

	mutex_lock(&aw->i2c_lock);
	if (packet->reg_addr == aw->aw_pa->dsp_mem_desc.dsp_mdat_reg) {
		if (packet->dsp_status != AWRW_DSP_READY) {
			aw_dev_err(aw->dev, "please write reg[0x40] first");
			mutex_unlock(&aw->i2c_lock);
			return -EINVAL;
		}
		ret = aw_i2c_write(aw, aw->aw_pa->dsp_mem_desc.dsp_madd_reg,
				   packet->dsp_addr);
		if (ret < 0) {
			mutex_unlock(&aw->i2c_lock);
			return ret;
		}
		packet->dsp_status = AWRW_DSP_ST_NONE;
	}

	data_len = AWRW_DATA_BYTES * packet->reg_num;
	reg_data = devm_kzalloc(dev, data_len, GFP_KERNEL);
	if (!reg_data) {
		aw_dev_err(aw->dev, "memory alloc failed");
		ret = -EINVAL;
		goto exit;
	}

	ret = aw_i2c_reads(aw, packet->reg_addr, (char *)reg_data, data_len);
	if (ret < 0) {
		ret = -EFAULT;
		goto exit;
	}

	aw_dev_info(aw->dev, "reg_addr 0x%02x, reg_num %d",
		    packet->reg_addr, packet->reg_num);

	for (i = 0; i < data_len; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,",
				reg_data[i]);
	ret = len;

exit:
	if (reg_data) {
		devm_kfree(dev, reg_data);
		reg_data = NULL;
	}
	mutex_unlock(&aw->i2c_lock);
	packet->i2c_status = AWRW_I2C_ST_NONE;
	return ret;
}

static ssize_t fade_step_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	int32_t databuf = 0;
	int ret = -1;

	if (kstrtoint(buf, 10, &databuf) == 0) {
		if (databuf > (aw->aw_pa->volume_desc.mute_volume)) {
			aw_dev_info(aw->dev, "step overflow %d", databuf);
			return count;
		}

		ret = aw_device_params(aw_dev, AW_DEV_FADE_STEP_PARAMS,
				       (void *)&databuf, sizeof(databuf),
				       AW_SET_DEV_PARAMS);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write_params failed");
			return ret;
		}
	}
	aw_dev_info(aw->dev, "set step %d Done", databuf);

	return count;
}

static ssize_t fade_step_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	u32 step = 0;
	ssize_t len = 0;
	int ret = -1;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;

	ret = aw_device_params(aw_dev, AW_DEV_FADE_STEP_PARAMS, (void *)&step,
			       sizeof(step), AW_GET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read_params failed");
		return ret;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "step: %u\n", step);

	return len;
}

static ssize_t dbg_prof_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	int32_t databuf = 0;

	if (kstrtoint(buf, 10, &databuf) == 0) {
		if (databuf)
			aw->dbg_en_prof = true;
		else
			aw->dbg_en_prof = false;
	}
	aw_dev_info(aw->dev, "en_prof %d  Done", databuf);

	return count;
}

static ssize_t dbg_prof_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, " %d\n", aw->dbg_en_prof);

	return len;
}

static ssize_t spk_temp_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	ssize_t len = 0;
	int ret = -1;
	int32_t te = 0;

	ret = aw_device_params(aw_dev, AW_DEV_TE_PARAMS, (void *)&te,
			       sizeof(te), AW_GET_DEV_PARAMS);
	if (ret < 0)
		return ret;

	len += snprintf(buf + len, PAGE_SIZE - len, "Temp:%d\n", te);

	return len;
}

static ssize_t phase_sync_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	unsigned int flag = 0;
	int ret = -1;

	ret = kstrtouint(buf, 0, &flag);
	if (ret < 0)
		return ret;

	flag = ((flag == false) ? false : true);

	aw_dev_info(aw->dev, "set phase sync flag : [%d]", flag);

	aw->phase_sync = flag;

	return count;
}

static ssize_t phase_sync_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "sync flag : %d\n",
			aw->phase_sync);

	return len;
}

static ssize_t fade_en_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	u32 fade_en = 0;

	if (kstrtouint(buf, 10, &fade_en) == 0)
		aw->aw_pa->fade_en = fade_en;

	aw_dev_info(aw->dev, "set fade_en %d", aw->aw_pa->fade_en);

	return count;
}

static ssize_t fade_en_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "fade_en: %d\n",
			aw->aw_pa->fade_en);

	return len;
}

static ssize_t dsp_re_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;
	int ret = -1;
	u32 re = 0;

	ret = aw_device_params(aw->aw_pa, AW_DEV_CALI_RE_PARAMS, (void *)&re,
			       sizeof(re), AW_GET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw->dev, "read dsp re fail");
		return ret;
	}

	len += snprintf((char *)(buf + len), PAGE_SIZE - len, "dsp_re: %u\n",
			re);

	return len;
}

static ssize_t i2c_log_en_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c_log_en: %d\n",
			aw->i2c_log_en);

	return len;
}

static ssize_t i2c_log_en_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	u32 log_en = 0;

	if (kstrtouint(buf, 10, &log_en) == 0)
		aw->i2c_log_en = log_en;

	aw_dev_info(aw->dev, "set i2c_log_en: %d", aw->i2c_log_en);

	return count;
}

static int aw_dsp_log_info(struct aw *aw, unsigned int base_addr,
			   u32 data_len, char *format)
{
	u16 reg_val = 0;
	char *dsp_reg_info = NULL;
	ssize_t dsp_info_len = 0;
	int i = 0;

	dsp_reg_info = devm_kzalloc(aw->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!dsp_reg_info)
		return -ENOMEM;

	mutex_lock(&aw->i2c_lock);
	aw_i2c_write(aw, aw->aw_pa->dsp_mem_desc.dsp_madd_reg, base_addr);

	for (i = 0; i < data_len; i += 2) {
		aw_i2c_read(aw, aw->aw_pa->dsp_mem_desc.dsp_mdat_reg,
			    &reg_val);
		dsp_info_len += snprintf(dsp_reg_info + dsp_info_len,
					 AW_NAME_BUF_MAX - dsp_info_len,
					 "%02x,%02x,",
					 (reg_val >> 0) & 0xff,
					 (reg_val >> 8) & 0xff);
		if ((i / 2 + 1) % 8 == 0) {
			aw_dev_info(aw->dev, "%s: %s", format, dsp_reg_info);
			dsp_info_len = 0;
			memset(dsp_reg_info, 0, AW_NAME_BUF_MAX);
		}

		if (((data_len) % 8 != 0) && (i == (data_len - 2))) {
			aw_dev_info(aw->dev, "%s: %s", format, dsp_reg_info);
			dsp_info_len = 0;
			memset(dsp_reg_info, 0, AW_NAME_BUF_MAX);
		}
	}

	dsp_info_len = 0;
	memset(dsp_reg_info, 0, AW_NAME_BUF_MAX);
	devm_kfree(aw->dev, dsp_reg_info);
	dsp_reg_info = NULL;
	mutex_unlock(&aw->i2c_lock);

	return 0;
}

static ssize_t dsp_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	struct aw_dsp_st *dsp_st_desc = &aw_dev->dsp_st_desc;
	ssize_t len = 0;
	int ret = -1;
	bool status = false;
	u32 data_len = 0;

	if (aw->aw_pa->dsp_cfg == AW_DEV_DSP_BYPASS) {
		len += snprintf((char *)(buf + len), PAGE_SIZE - len,
				"%s: dsp bypass\n", __func__);
	} else {
		len += snprintf((char *)(buf + len), PAGE_SIZE - len,
				"%s: dsp working\n", __func__);

		/* get pa status */
		status = aw_device_status(aw_dev, AW_DEV_PLL_WDT_STATUS,
					  AW_GET_DEV_STATUS);
		if (!status) {
			len += snprintf((char *)(buf + len), PAGE_SIZE - len,
					"%s: PA status abnormal\n", __func__);
			aw_dev_err(aw->dev,
				   "PA status abnormal, dsp show failed");
			return len;
		}

		len += snprintf(buf + len, PAGE_SIZE - len,
				"dsp firmware and config info is displayed in the kernel log\n");

		aw_dev_info(aw->dev, "dsp_firmware_len:%d",
			    aw->aw_pa->dsp_fw_len);
		ret = aw_dsp_log_info(aw,
				      aw->aw_pa->dsp_mem_desc.dsp_fw_base_addr,
				      aw->aw_pa->dsp_fw_len, "dsp_fw");
		if (ret < 0) {
			aw_dev_err(aw->dev, "dsp_fw display failed");
			return len;
		}

		aw_dev_info(aw->dev, "dsp_config_len:%d",
			    aw->aw_pa->dsp_cfg_len);
		ret = aw_dsp_log_info(aw,
				      aw->aw_pa->dsp_mem_desc.dsp_cfg_base_addr,
				      aw->aw_pa->dsp_cfg_len, "dsp_config");
		if (ret < 0) {
			aw_dev_err(aw->dev, "dsp_config display failed");
			return len;
		}

		aw_dev_info(aw->dev, "dsp_config:0x%04x-0x%04x",
			    dsp_st_desc->dsp_reg_s1, dsp_st_desc->dsp_reg_e1);
		data_len = (u32)(2 * (u32)(dsp_st_desc->dsp_reg_e1 -
					    dsp_st_desc->dsp_reg_s1));
		ret = aw_dsp_log_info(aw, dsp_st_desc->dsp_reg_s1,
				      data_len, "dsp_st");
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "dsp_config:0x%04x-0x%04x failed",
				   dsp_st_desc->dsp_reg_s1,
				   dsp_st_desc->dsp_reg_e1);
			return len;
		}

		aw_dev_info(aw->dev, "dsp_config:0x%04x-0x%04x",
			    dsp_st_desc->dsp_reg_s2, dsp_st_desc->dsp_reg_e2);
		data_len = (u32)(2 * (u32)(dsp_st_desc->dsp_reg_e2 -
					    dsp_st_desc->dsp_reg_s2));
		ret = aw_dsp_log_info(aw, dsp_st_desc->dsp_reg_s2,
				      data_len, "dsp_st");
		if (ret < 0) {
			aw_dev_err(aw->dev,
				   "dsp_config:0x%04x-0x%04x display failed",
				   dsp_st_desc->dsp_reg_s2,
				   dsp_st_desc->dsp_reg_e2);
			return len;
		}
	}

	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(rw);
static DEVICE_ATTR_RO(drv_ver);
static DEVICE_ATTR_RW(dsp_rw);
static DEVICE_ATTR_RW(awrw);
static DEVICE_ATTR_RW(fade_step);
static DEVICE_ATTR_RW(dbg_prof);
static DEVICE_ATTR_RO(spk_temp);
static DEVICE_ATTR_RW(phase_sync);
static DEVICE_ATTR_RW(fade_en);
static DEVICE_ATTR_RO(dsp_re);
static DEVICE_ATTR_RW(i2c_log_en);
static DEVICE_ATTR_RO(dsp);

static struct attribute *aw_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_rw.attr,
	&dev_attr_drv_ver.attr,
	&dev_attr_dsp_rw.attr,
	&dev_attr_awrw.attr,
	&dev_attr_fade_step.attr,
	&dev_attr_dbg_prof.attr,
	&dev_attr_spk_temp.attr,
	&dev_attr_phase_sync.attr,
	&dev_attr_fade_en.attr,
	&dev_attr_dsp_re.attr,
	&dev_attr_i2c_log_en.attr,
	&dev_attr_dsp.attr,
	NULL,
};

static struct attribute_group aw_attribute_group = {
	.attrs = aw_attributes,
};

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw_i2c_probe(struct i2c_client *i2c)
{
	struct aw *aw = NULL;
	int ret = -1;

	aw_dev_info(&i2c->dev, "enter");

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_dev_err(&i2c->dev, "check_functionality failed");
		return -EIO;
	}

	aw = aw_malloc_init(i2c);
	if (!aw) {
		aw_dev_err(&i2c->dev, "malloc aw failed");
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, aw);

	ret = aw_parse_dt(aw);
	if (ret < 0) {
		aw_dev_err(&i2c->dev, "parse dts failed");
		return ret;
	}

	/*get gpio resource*/
	ret = aw_gpio_request(aw);
	if (ret)
		return ret;

	/* hardware reset */
	aw_hw_reset(aw);

	/* aw chip id */
	ret = aw_match_chipid(aw);
	if (ret < 0) {
		aw_dev_err(&i2c->dev, "aw_match_chipid failed ret=%d", ret);
		return ret;
	}

	/*aw pa init*/
	ret = aw_init(aw);
	if (ret < 0)
		return ret;

	ret = aw_interrupt_init(aw);
	if (ret < 0)
		return ret;

	ret = aw_componet_codec_register(aw);
	if (ret) {
		aw_dev_err(&i2c->dev, "codec register failed");
		return ret;
	}

	ret = sysfs_create_group(&i2c->dev.kobj, &aw_attribute_group);
	if (ret < 0) {
		aw_dev_info(&i2c->dev,
			    "error creating sysfs attr files");
		goto err_sysfs;
	}

	dev_set_drvdata(&i2c->dev, aw);

	/*add device to total list*/
	mutex_lock(&g_aw_lock);
	g_aw_dev_cnt++;
	aw_dev_add_dev_list(aw->aw_pa);
	mutex_unlock(&g_aw_lock);

	aw_dev_info(&i2c->dev, "dev_cnt %d probe completed successfully",
		    g_aw_dev_cnt);

	return 0;

err_sysfs:
	aw_componet_codec_ops.unregister_codec(&i2c->dev);
	return ret;
}

#if defined AW_KERNEL_VER_OVER_6_1_0
static void aw_i2c_remove(struct i2c_client *i2c)
#else
static int aw_i2c_remove(struct i2c_client *i2c)
#endif
{
	struct aw *aw = i2c_get_clientdata(i2c);

	aw_dev_info(aw->dev, "enter");

	if (gpio_to_irq(aw->irq_gpio))
		devm_free_irq(&i2c->dev, gpio_to_irq(aw->irq_gpio), aw);

	sysfs_remove_group(&aw->dev->kobj, &aw_attribute_group);

	/*free device resource */
	aw_device_remove(aw->aw_pa);

	aw_componet_codec_ops.unregister_codec(&i2c->dev);

	mutex_lock(&g_aw_lock);
	g_aw_dev_cnt--;
	if (g_aw_dev_cnt == 0) {
		vfree(g_awinic_cfg);
		g_awinic_cfg = NULL;
	}
	mutex_unlock(&g_aw_lock);

#if !defined AW_KERNEL_VER_OVER_6_1_0
	return 0;
#endif
}

static const struct i2c_device_id aw_i2c_id[] = {
	{AW_I2C_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw_i2c_id);

static const struct of_device_id aw_dt_match[] = {
	{ .compatible = "awinic,aw881xx_smartpa" },
	{},
};

static struct i2c_driver aw_i2c_driver = {
	.driver = {
		.name = AW_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw_dt_match),
	},
	.probe = aw_i2c_probe,
	.remove = aw_i2c_remove,
	.id_table = aw_i2c_id,
};

static int __init aw_i2c_init(void)
{
	int ret = -1;

	aw_pr_info("aw driver version %s", AW_DRIVER_VERSION);

	ret = i2c_add_driver(&aw_i2c_driver);
	if (ret)
		aw_pr_err("fail to add aw device into i2c");

	return ret;
}
module_init(aw_i2c_init);

static void __exit aw_i2c_exit(void)
{
	i2c_del_driver(&aw_i2c_driver);
}
module_exit(aw_i2c_exit);

MODULE_DESCRIPTION("ASoC AW Smart PA Driver");
MODULE_LICENSE("GPL");
