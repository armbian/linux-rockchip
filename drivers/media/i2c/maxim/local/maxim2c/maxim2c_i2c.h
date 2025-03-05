/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __MAXIM2C_I2C_H__
#define __MAXIM2C_I2C_H__

#include <linux/i2c.h>
#include <linux/i2c-mux.h>

/* register address: 8bit or 16bit */
#define MAXIM2C_I2C_REG_ADDR_08BITS	1
#define MAXIM2C_I2C_REG_ADDR_16BITS	2

/* register value: 8bit or 16bit or 24bit */
#define MAXIM2C_I2C_REG_VALUE_08BITS	1
#define MAXIM2C_I2C_REG_VALUE_16BITS	2
#define MAXIM2C_I2C_REG_VALUE_24BITS	3

/* I2C Device ID */
enum {
	MAXIM2C_I2C_DES_DEF,	/* Deserializer I2C address: Default */

	MAXIM2C_I2C_SER_DEF,	/* Serializer I2C address: Default */
	MAXIM2C_I2C_SER_MAP,	/* Serializer I2C address: Mapping */

	MAXIM2C_I2C_CAM_DEF,	/* Camera I2C address: Default */
	MAXIM2C_I2C_CAM_MAP,	/* Camera I2C address: Mapping */

	MAXIM2C_I2C_DEV_MAX,
};

struct maxim4c_i2c_mux {
	struct i2c_mux_core *muxc;
	u32 i2c_mux_mask;
	u32 mux_channel;
	bool mux_disable;
};

/* i2c register array end */
#define MAXIM2C_REG_NULL		0xFFFF

struct maxim2c_i2c_regval {
	u16 reg_len;
	u16 reg_addr;
	u32 val_len;
	u32 reg_val;
	u32 val_mask;
	u8 delay;
};

/* seq_item_size = reg_len + val_len * 2 + 1 */
struct maxim2c_i2c_init_seq {
	struct maxim2c_i2c_regval *reg_init_seq;
	u32 reg_seq_size;
	u32 seq_item_size;
	u32 reg_len;
	u32 val_len;
};

/* maxim2c i2c read/write api */
int maxim2c_i2c_write(struct i2c_client *client,
		u16 reg_addr, u16 reg_len, u32 val_len, u32 reg_val);
int maxim2c_i2c_read(struct i2c_client *client,
		u16 reg_addr, u16 reg_len, u32 val_len, u32 *reg_val);
int maxim2c_i2c_update(struct i2c_client *client,
		u16 reg_addr, u16 reg_len,
		u32 val_len, u32 val_mask, u32 reg_val);

int maxim2c_i2c_write_reg(struct i2c_client *client,
		u16 reg_addr, u8 reg_val);
int maxim2c_i2c_read_reg(struct i2c_client *client,
		u16 reg_addr, u8 *reg_val);
int maxim2c_i2c_update_reg(struct i2c_client *client,
		u16 reg_addr, u8 val_mask, u8 reg_val);

int maxim2c_i2c_write_array(struct i2c_client *client,
			const struct maxim2c_i2c_regval *regs);
int maxim2c_i2c_load_init_seq(struct device *dev,
	struct device_node *node, struct maxim2c_i2c_init_seq *init_seq);
int maxim2c_i2c_run_init_seq(struct i2c_client *client,
			struct maxim2c_i2c_init_seq *init_seq);

#endif /* __MAXIM2C_I2C_H__ */
