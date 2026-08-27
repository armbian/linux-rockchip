/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Shared definitions of the Rockchip SPI object master/slave protocol.
 */

#ifndef __RKSPI_OBJ_H
#define __RKSPI_OBJ_H

#define SPI_OBJ_MAX_XFER_SIZE	0x1040
#define SPI_OBJ_APP_RAM_SIZE	0x10000

#define SPI_OBJ_CTRL_MSG_SIZE	0x8
#define SPI_OBJ_CTRL_CMD_INIT	0x99
#define SPI_OBJ_CTRL_CMD_READ	0x3A
#define SPI_OBJ_CTRL_CMD_WRITE	0x4B
#define SPI_OBJ_CTRL_CMD_DUPLEX	0x5C

struct spi_obj_ctrl {
	u16 cmd;
	u16 addr;
	u32 data;
};

/**
 * spidev_obj_param_valid - validate addr/len of a data phase against APP_RAM
 * @addr: start offset in APP_RAM, parsed from userspace as signed int
 * @len: transfer length, parsed from userspace as signed int
 * @max_len: upper limit of len, 0 means no extra limit beyond APP_RAM size
 *
 * Rejects negative values and checks the range by subtraction instead of
 * "addr + len", so that the check cannot overflow near INT_MAX.
 */
static inline bool spidev_obj_param_valid(int addr, int len, int max_len)
{
	if (len <= 0 || addr < 0 ||
	    addr > SPI_OBJ_APP_RAM_SIZE - len)
		return false;

	if (max_len && len > max_len)
		return false;

	return true;
}

#endif /* __RKSPI_OBJ_H */
