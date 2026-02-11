/* SPDX-License-Identifier: GPL-2.0
 * aw.h   aw codec driver
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

#ifndef __AW_H__
#define __AW_H__

#include <linux/version.h>
#include <sound/control.h>
#include <sound/soc.h>

#include "aw_device.h"

/*#define AW_QCOM_PLATFORM*/
/*#define AW_MTK_PLATFORM*/
#define AW_SPRD_PLATFORM

#define AW_CHIP_ID_REG (0x00)

#if KERNEL_VERSION(4, 19, 1) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_4_19_1
#endif

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_5_4_0
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_5_10_0
#endif
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_6_1_0
#endif

/* i2c transaction on Linux limited to 64k
 * (See Linux kernel documentation: Documentation/i2c/writing-clients)
 */
#define MAX_I2C_BUFFER_SIZE        (65536)
#define AW_READ_MSG_NUM            (2)
#define AW_I2C_RETRIES             (5)
#define AW_I2C_RETRY_DELAY         (5) /* 5ms */
#define AW_READ_CHIPID_RETRY_DELAY (5) /* 5ms */
#define AW_START_RETRIES           (5)
#define AW_FLAG_START_ON_MUTE      (1 << 0)
#define AW_FLAG_SKIP_INTERRUPTS    (1 << 1)
#define AW_I2S_CHECK_MAX           (5)
#define AW_SYSST_CHECK_MAX         (10)
#define AW_BIN_TYPE_NUM            (3)
#define AW_LOAD_FW_DELAY_TIME      (3000)
#define AW_START_WORK_DELAY_MS     (0)
#define AW_DSP_16_DATA_MASK        (0x0000ffff)
#define AW_GET_IV_CNT_MAX          (6)
#define AW_KCONTROL_NUM            (4)
#define AW_DSP_MONITOR_DELAY       (1000)

enum aw_id {
	AW_PID_2049 = 0x2049,
	AW_PID_2066 = 0x2066,
	AW_PID_2183 = 0x2183,
};

enum {
	AWRW_I2C_ST_NONE = 0,
	AWRW_I2C_ST_READ,
	AWRW_I2C_ST_WRITE,
};

enum {
	AWRW_DSP_ST_NONE = 0,
	AWRW_DSP_READY,
};

enum {
	AW_SYNC_START = 0,
	AW_ASYNC_START,
};

#define AWRW_ADDR_BYTES (1)
#define AWRW_DATA_BYTES (2)
#define AWRW_HDR_LEN    (24)

enum {
	AWRW_FLAG_WRITE = 0,
	AWRW_FLAG_READ,
};

enum {
	AWRW_HDR_WR_FLAG = 0,
	AWRW_HDR_ADDR_BYTES,
	AWRW_HDR_DATA_BYTES,
	AWRW_HDR_REG_NUM,
	AWRW_HDR_REG_ADDR,
	AWRW_HDR_MAX,
};

struct aw_i2c_packet {
	unsigned char i2c_status;
	unsigned char dsp_status;
	unsigned int reg_num;
	unsigned int reg_addr;
	unsigned int dsp_addr;
	char *reg_data;
};

enum {
	AW_STREAM_CLOSE = 0,
	AW_STREAM_OPEN,
};

enum aw_init {
	AW_INIT_ST = 0,
	AW_INIT_OK = 1,
	AW_INIT_NG = 2,
};

enum aw_re_range {
	AW_RE_MIN = 1000,
	AW_RE_MAX = 40000,
};

enum {
	AW_RENAME_DISABLE = 0,
	AW_RENAME_ENABLE,
};

/********************************************
 *
 * Compatible with codec and component
 *
 *******************************************/
#ifdef AW_KERNEL_VER_OVER_4_19_1
typedef struct snd_soc_component aw_snd_soc_codec_t;
typedef const struct snd_soc_component_driver aw_snd_soc_codec_driver_t;
#else
typedef struct snd_soc_codec aw_snd_soc_codec_t;
typedef const struct snd_soc_codec_driver aw_snd_soc_codec_driver_t;
#endif

struct aw_componet_codec_ops {
	aw_snd_soc_codec_t *(*kcontrol_codec)(struct snd_kcontrol *kcontrol);
	void *(*codec_get_drvdata)(aw_snd_soc_codec_t *codec);
	int (*add_codec_controls)(aw_snd_soc_codec_t *codec,
				   const struct snd_kcontrol_new *controls,
				   unsigned int num_controls);
	void (*unregister_codec)(struct device *dev);
	int (*register_codec)(struct device *dev,
			      const aw_snd_soc_codec_driver_t *codec_drv,
			      struct snd_soc_dai_driver *dai_drv,
			      int num_dai);
};

struct aw {
	struct i2c_client *i2c;
	struct device *dev;
	struct mutex lock;
	struct mutex i2c_lock;
	aw_snd_soc_codec_t *codec;
	struct aw_componet_codec_ops *codec_ops;
	struct aw_device *aw_pa;

	int sysclk;
	int reset_gpio;
	int irq_gpio;

	unsigned char phase_sync; /*phase sync*/
	u32 allow_pw;
	u32 rename_flag;
	unsigned char sync_load; /*sync load fw*/
	u8 pstream;
	unsigned char fw_retry_cnt;

	u8 dbg_en_prof;
	u8 i2c_log_en;
	u8 spin_flag;

	struct list_head list;

	struct workqueue_struct *work_queue;
	struct delayed_work start_work;
	struct delayed_work monitor_work;
	struct delayed_work interrupt_work;
	struct delayed_work acf_work;

	u8 reg_addr;
	u16 dsp_addr;
	u16 chip_id;
	struct aw_i2c_packet i2c_packet;
};

int aw_init(struct aw *aw);
int aw_init_check_chipid(struct aw *aw, u16 chipid);
int aw_pid_2049_dev_init(struct aw_device *aw_pa);
int aw_pid_2066_dev_init(struct aw_device *aw_pa);
int aw_pid_2183_dev_init(struct aw_device *aw_pa);

int aw_reg_write(struct aw *aw, u8 reg_addr, u16 reg_data);
int aw_reg_read(struct aw *aw, u8 reg_addr, u16 *reg_data);
int aw_reg_write_bits(struct aw *aw, u8 reg_addr, u16 mask, u16 reg_data);
int aw_dsp_write_bits(struct aw *aw, u16 dsp_addr, u32 dsp_mask,
		      u32 dsp_data, u8 data_type);
int aw_dsp_write(struct aw *aw, u16 dsp_addr, u32 dsp_data, u8 data_type);
int aw_dsp_writes(struct aw *aw, u8 *data, u32 len, u16 base);
int aw_dsp_read(struct aw *aw, u16 dsp_addr, u32 *dsp_data, u8 data_type);
int aw_get_dev_num(void);
int aw_get_version(char *buf, int size);

#endif
