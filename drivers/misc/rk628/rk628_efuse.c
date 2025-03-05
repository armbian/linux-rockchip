// SPDX-License-Identifier: GPL-2.0
/*
 * RK628 eFuse Driver
 *
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Weixin Zhou <zwx@rock-chips.com>
 */

#include <linux/gpio/consumer.h>
#include "rk628.h"
#include "rk628_efuse.h"

#define EFUSE_SIZE		64

#define T_CSB_P_S		0
#define T_PGENB_P_S		(15 + 200)
#define T_LOAD_P_S		0
#define T_ADDR_P_S		(15 + 200 + 5)
#define T_STROBE_P_S		((150 + 2000 + 100) / 9)
#define T_CSB_P_L		0
#define T_PGENB_P_L		(15 + 200 + 10 + 200 + 190 + 10)
#define T_LOAD_P_L		(15 + 200 + 200 + 190 + 10 + 100 + 15)
#define T_ADDR_P_L		(15 + 200 + 5 + 200 + 5)
#define T_STROBE_P_L		((150 + 2000 + 100 + 2000) / 9)
#define T_CSB_R_S		0
#define T_PGENB_R_S		0
#define T_LOAD_R_S		15
#define T_ADDR_R_S		(15 + 9)
#define T_STROBE_R_S		((150 + 100) / 9)
#define T_CSB_R_L		0
#define T_PGENB_R_L		0
#define T_LOAD_R_L		(15 + 5 + 5 + 10 + 15)
#define T_ADDR_R_L		(15 + 10 + 5 + 1)
#define T_STROBE_R_L		((150 + 100 + 50) / 8)

#define T_CSB_P			0x28
#define T_PGENB_P		0x2c
#define T_LOAD_P		0x30
#define T_ADDR_P		0x34
#define T_STROBE_P		0x38
#define T_CSB_R			0x3c
#define T_PGENB_R		0x40
#define T_LOAD_R		0x44
#define T_ADDR_R		0x48
#define T_STROBE_R		0x4c

#define RK628_EFUSE_BASE	0xb0000
#define RK628_MOD		0x00
#define RK628_INT_STATUS	0x0018
#define RK628_DOUT		0x0020
#define RK628_AUTO_CTRL		0x0024
#define RK628_USER_MODE		BIT(0)
#define RK628_INT_FINISH	BIT(0)
#define RK628_AUTO_ENB		BIT(0)
#define RK628_AUTO_RD		BIT(1)
#define RK628_ADDR_ROW		16
#define RK628_ADDR_COL		22
#define RK628_A_SHIFT		16
#define RK628_A_MASK		0x3ff
#define RK628_NBYTES		1

#define REG_EFUSE_CTRL		0x0000
#define REG_EFUSE_DOUT		0x0004

static inline u32 rk628_read(struct rk628 *rk628, u32 reg)
{
	u32 val;

	rk628_i2c_read(rk628, reg, &val);

	return val;
}

static inline void rk628_write(struct rk628 *rk628, u32 val, u32 reg)
{
	rk628_i2c_write(rk628, reg, val);
}

static inline void rk628_efuse_timing_init(struct rk628 *rk628)
{
	u32 base = RK628_EFUSE_BASE;
	/* enable auto mode */
	rk628_write(rk628, rk628_read(rk628, base + RK628_MOD)
			& (~RK628_USER_MODE), base + RK628_MOD);

	/* setup efuse timing */
	rk628_write(rk628, (T_CSB_P_S << 16) | T_CSB_P_L, base + T_CSB_P);
	rk628_write(rk628, (T_PGENB_P_S << 16) | T_PGENB_P_L, base + T_PGENB_P);
	rk628_write(rk628, (T_LOAD_P_S << 16) | T_LOAD_P_L, base + T_LOAD_P);
	rk628_write(rk628, (T_ADDR_P_S << 16) | T_ADDR_P_L, base + T_ADDR_P);
	rk628_write(rk628, (T_STROBE_P_S << 16) | T_STROBE_P_L, base + T_STROBE_P);
	rk628_write(rk628, (T_CSB_R_S << 16) | T_CSB_R_L, base + T_CSB_R);
	rk628_write(rk628, (T_PGENB_R_S << 16) | T_PGENB_R_L, base + T_PGENB_R);
	rk628_write(rk628, (T_LOAD_R_S << 16) | T_LOAD_R_L, base + T_LOAD_R);
	rk628_write(rk628, (T_ADDR_R_S << 16) | T_ADDR_R_L, base + T_ADDR_R);
	rk628_write(rk628, (T_STROBE_R_S << 16) | T_STROBE_R_L, base + T_STROBE_R);
}

static inline void rk628_efuse_timing_deinit(struct rk628 *rk628)
{
	u32 base = RK628_EFUSE_BASE;
	/* disable auto mode */
	rk628_write(rk628, rk628_read(rk628, base + RK628_MOD)
			| RK628_USER_MODE, base + RK628_MOD);

	/* clear efuse timing */
	rk628_write(rk628, 0, base + T_CSB_P);
	rk628_write(rk628, 0, base + T_PGENB_P);
	rk628_write(rk628, 0, base + T_LOAD_P);
	rk628_write(rk628, 0, base + T_ADDR_P);
	rk628_write(rk628, 0, base + T_STROBE_P);
	rk628_write(rk628, 0, base + T_CSB_R);
	rk628_write(rk628, 0, base + T_PGENB_R);
	rk628_write(rk628, 0, base + T_LOAD_R);
	rk628_write(rk628, 0, base + T_ADDR_R);
	rk628_write(rk628, 0, base + T_STROBE_R);
}

int rk628_efuse_read(struct rk628 *rk628, unsigned int offset,
		     void *val, size_t bytes)
{
	unsigned int addr_start, addr_end, addr_offset, addr_len;
	u32 out_value, status;
	u8 *buf;
	int ret = 0, i = 0;

	addr_start = rounddown(offset, RK628_NBYTES) / RK628_NBYTES;
	addr_end = roundup(offset + bytes, RK628_NBYTES) / RK628_NBYTES;
	addr_offset = offset % RK628_NBYTES;
	addr_len = addr_end - addr_start;

	if (addr_len == 0 || addr_len > EFUSE_SIZE)
		return -EINVAL;

	buf = kzalloc(sizeof(*buf) * addr_len * RK628_NBYTES, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rk628_efuse_timing_init(rk628);

	while (addr_len--) {
		rk628_write(rk628, RK628_AUTO_RD | RK628_AUTO_ENB |
		       ((addr_start++ & RK628_A_MASK) << RK628_A_SHIFT),
		       RK628_EFUSE_BASE + RK628_AUTO_CTRL);
		udelay(2);
		status = rk628_read(rk628, RK628_EFUSE_BASE + RK628_INT_STATUS);
		if (!(status & RK628_INT_FINISH)) {
			ret = -EIO;
			goto err;
		}
		out_value = rk628_read(rk628, RK628_EFUSE_BASE + RK628_DOUT);
		rk628_write(rk628, RK628_INT_FINISH, RK628_EFUSE_BASE + RK628_INT_STATUS);

		memcpy(&buf[i], &out_value, RK628_NBYTES);
		i += RK628_NBYTES;
	}
	memcpy(val, buf + addr_offset, bytes);
err:
	rk628_efuse_timing_deinit(rk628);
	kfree(buf);

	return ret;
}
