// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info zbit_parts[] = {
	{ "ZB25VQ64", INFO(0x5e4017, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "ZB25VQ128", INFO(0x5e4018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "ZB25LQ128", INFO(0x5e5018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
};

const struct spi_nor_manufacturer spi_nor_zbit = {
	.name = "zbit",
	.parts = zbit_parts,
	.nparts = ARRAY_SIZE(zbit_parts),
};
