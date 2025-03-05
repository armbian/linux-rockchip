/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __MAXIM_REMOTE_H__
#define __MAXIM_REMOTE_H__

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/version.h>

#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
enum rkmodule_pad_type {
	PAD0,
	PAD1,
	PAD2,
	PAD3,
	PAD_MAX,
};

#ifndef fallthrough
#define fallthrough
#endif
#endif /* LINUX_VERSION_CODE */

/* Serializer State */
#define MAXIM_REMOTE_SER_DEINIT		0
#define MAXIM_REMOTE_SER_INIT		1

/* I2C Device ID */
enum {
	MAXIM_REMOTE_I2C_SER_DEF,	/* Serializer I2C address: Default */
	MAXIM_REMOTE_I2C_SER_MAP,	/* Serializer I2C address: Mapping */

	MAXIM_REMOTE_I2C_CAM_DEF,	/* Camera I2C address: Default */
	MAXIM_REMOTE_I2C_CAM_MAP,	/* Camera I2C address: Mapping */

	MAXIM_REMOTE_I2C_DEV_MAX,
};

/* i2c register array end */
#define MAXIM_REMOTE_REG_NULL		0xFFFF

struct maxim_remote_regval {
	u16 reg_len;
	u16 reg_addr;
	u32 val_len;
	u32 reg_val;
	u32 val_mask;
	u8 delay;
};

/* seq_item_size = reg_len + val_len * 2 + 1 */
struct maxim_remote_init_seq {
	struct maxim_remote_regval *reg_init_seq;
	u32 reg_seq_size;
	u32 seq_item_size;
	u32 reg_len;
	u32 val_len;
};

struct maxim_remote_ser;

struct maxim_remote_ser_ops {
	int (*ser_module_init)(struct maxim_remote_ser *remote_ser);
	int (*ser_module_deinit)(struct maxim_remote_ser *remote_ser);
	int (*ser_pclk_detect)(struct maxim_remote_ser *remote_ser);
};

typedef struct maxim_remote_ser {
	struct i2c_client *client;

	struct mutex mutex;

	u32 chip_id;

	u8 ser_i2c_addr_def;
	u8 ser_i2c_addr_map;

	u8 cam_i2c_addr_def;
	u8 cam_i2c_addr_map;

	u32 ser_state;
	struct maxim_remote_init_seq ser_init_seq;
	const struct maxim_remote_ser_ops *ser_ops;
} maxim_remote_ser_t;

static inline int maxim_remote_parse_init_seq(struct device *dev,
		const u8 *seq_data, int data_len, struct maxim_remote_init_seq *init_seq)
{
	struct maxim_remote_regval *reg_val = NULL;
	u8 *data_buf = NULL, *d8 = NULL;
	u32 i = 0;

	if ((seq_data == NULL) || (init_seq == NULL)) {
		dev_err(dev, "%s: input parameter = NULL\n", __func__);
		return -EINVAL;
	}

	if ((init_seq->seq_item_size == 0)
			|| (data_len == 0)
			|| (init_seq->reg_len == 0)
			|| (init_seq->val_len == 0)) {
		dev_err(dev, "%s: input parameter size zero\n", __func__);
		return -EINVAL;
	}

	// data_len = seq_item_size * N
	if (data_len % init_seq->seq_item_size) {
		dev_err(dev, "%s: data_len or seq_item_size error\n", __func__);
		return -EINVAL;
	}

	// seq_item_size = reg_len + val_len * 2 + 1
	if (init_seq->seq_item_size !=
			(init_seq->reg_len + init_seq->val_len * 2 + 1)) {
		dev_err(dev, "%s: seq_item_size or reg_len or val_len error\n", __func__);
		return -EINVAL;
	}

	data_buf = devm_kmemdup(dev, seq_data, data_len, GFP_KERNEL);
	if (!data_buf) {
		dev_err(dev, "%s data buf error\n", __func__);
		return -ENOMEM;
	}

	d8 = data_buf;

	init_seq->reg_seq_size = data_len / init_seq->seq_item_size;
	init_seq->reg_seq_size += 1; // add 1 for end register setting

	init_seq->reg_init_seq = devm_kcalloc(dev, init_seq->reg_seq_size,
					sizeof(struct maxim_remote_regval), GFP_KERNEL);
	if (!init_seq->reg_init_seq) {
		dev_err(dev, "%s init seq buffer error\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < init_seq->reg_seq_size - 1; i++) {
		reg_val = &init_seq->reg_init_seq[i];

		reg_val->reg_len = init_seq->reg_len;
		reg_val->val_len = init_seq->val_len;

		reg_val->reg_addr = 0;
		switch (init_seq->reg_len) {
		case 4:
			reg_val->reg_addr |= (*d8 << 24);
			d8 += 1;
			fallthrough;
		case 3:
			reg_val->reg_addr |= (*d8 << 16);
			d8 += 1;
			fallthrough;
		case 2:
			reg_val->reg_addr |= (*d8 << 8);
			d8 += 1;
			fallthrough;
		case 1:
			reg_val->reg_addr |= (*d8 << 0);
			d8 += 1;
			break;
		}

		reg_val->reg_val = 0;
		switch (init_seq->val_len) {
		case 4:
			reg_val->reg_val |= (*d8 << 24);
			d8 += 1;
			fallthrough;
		case 3:
			reg_val->reg_val |= (*d8 << 16);
			d8 += 1;
			fallthrough;
		case 2:
			reg_val->reg_val |= (*d8 << 8);
			d8 += 1;
			fallthrough;
		case 1:
			reg_val->reg_val |= (*d8 << 0);
			d8 += 1;
			break;
		}

		reg_val->val_mask = 0;
		switch (init_seq->val_len) {
		case 4:
			reg_val->val_mask |= (*d8 << 24);
			d8 += 1;
			fallthrough;
		case 3:
			reg_val->val_mask |= (*d8 << 16);
			d8 += 1;
			fallthrough;
		case 2:
			reg_val->val_mask |= (*d8 << 8);
			d8 += 1;
			fallthrough;
		case 1:
			reg_val->val_mask |= (*d8 << 0);
			d8 += 1;
			break;
		}

		reg_val->delay = *d8;
		d8 += 1;
	}

	// End register setting
	init_seq->reg_init_seq[init_seq->reg_seq_size - 1].reg_len = init_seq->reg_len;
	init_seq->reg_init_seq[init_seq->reg_seq_size - 1].reg_addr = MAXIM_REMOTE_REG_NULL;

	return 0;
}

static inline int maxim_remote_load_init_seq(struct device *dev,
		struct device_node *node, struct maxim_remote_init_seq *init_seq)
{
	const void *init_seq_data = NULL;
	u32 seq_data_len = 0, value = 0;
	int ret = 0;

	if ((node == NULL) || (init_seq == NULL)) {
		dev_err(dev, "%s input parameter error\n", __func__);
		return -EINVAL;
	}

	init_seq->reg_init_seq = NULL;
	init_seq->reg_seq_size = 0;

	if (!of_device_is_available(node)) {
		dev_info(dev, "%pOF is disabled\n", node);

		return 0;
	}

	init_seq_data = of_get_property(node, "init-sequence", &seq_data_len);
	if (!init_seq_data) {
		dev_err(dev, "failed to get property init-sequence\n");
		return -EINVAL;
	}
	if (seq_data_len == 0) {
		dev_err(dev, "init-sequence date is empty\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "seq-item-size", &value);
	if (ret) {
		dev_err(dev, "failed to get property seq-item-size\n");
		return -EINVAL;
	} else {
		dev_info(dev, "seq-item-size property: %d", value);
		init_seq->seq_item_size = value;
	}

	ret = of_property_read_u32(node, "reg-addr-len", &value);
	if (ret) {
		dev_err(dev, "failed to get property reg-addr-len\n");
		return -EINVAL;
	} else {
		dev_info(dev, "reg-addr-len property: %d", value);
		init_seq->reg_len = value;
	}

	ret = of_property_read_u32(node, "reg-val-len", &value);
	if (ret) {
		dev_err(dev, "failed to get property reg-val-len\n");
		return -EINVAL;
	} else {
		dev_info(dev, "reg-val-len property: %d", value);
		init_seq->val_len = value;
	}

	ret = maxim_remote_parse_init_seq(dev,
			init_seq_data, seq_data_len, init_seq);
	if (ret) {
		dev_err(dev, "failed to parse init-sequence\n");
		return ret;
	}

	return 0;
}

static inline int maxim_remote_ser_i2c_addr_select(maxim_remote_ser_t *remote_ser, u32 i2c_id)
{
	struct i2c_client *ser_client = remote_ser->client;
	struct device *dev = &ser_client->dev;

	if (i2c_id == MAXIM_REMOTE_I2C_SER_DEF) {
		ser_client->addr = remote_ser->ser_i2c_addr_def;
		dev_info(dev, "ser select default i2c addr = 0x%02x\n", ser_client->addr);
	} else if (i2c_id == MAXIM_REMOTE_I2C_SER_MAP) {
		ser_client->addr = remote_ser->ser_i2c_addr_map;
		dev_info(dev, "ser select mapping i2c addr = 0x%02x\n", ser_client->addr);
	} else {
		dev_err(dev, "i2c select id = %d error\n", i2c_id);
		return -EINVAL;
	}

	return 0;
}

static inline struct maxim_remote_ser *maxim_remote_cam_bind_ser(struct device *cam_dev)
{
	struct i2c_client *ser_client = NULL;
	struct device_node *ser_node = NULL;
	maxim_remote_ser_t *remote_ser = NULL;

	/* camera get remote serializer node */
	ser_node = of_parse_phandle(cam_dev->of_node, "cam-remote-ser", 0);
	if (!IS_ERR_OR_NULL(ser_node)) {
		dev_info(cam_dev, "remote serializer node: %pOF\n", ser_node);

		ser_client = of_find_i2c_device_by_node(ser_node);
		of_node_put(ser_node);
		if (!IS_ERR_OR_NULL(ser_client)) {
			remote_ser = i2c_get_clientdata(ser_client);
			if (!IS_ERR_OR_NULL(remote_ser))
				return remote_ser;
			else
				return NULL;
		} else {
			dev_err(cam_dev, "camera find remote serializer client error\n");

			return NULL;
		}
	} else {
		dev_warn(cam_dev, "cam-remote-ser node isn't exist\n");
		return NULL;
	}
}

static inline bool maxim_remote_ser_is_inited(maxim_remote_ser_t *remote_ser)
{
	if (remote_ser && (remote_ser->ser_state == MAXIM_REMOTE_SER_INIT))
		return true;

	return false;
}
#endif /* __MAXIM_REMOTE_H__ */
