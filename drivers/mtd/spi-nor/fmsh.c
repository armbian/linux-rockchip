// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info fmsh_parts[] = {
	{
		.id = SNOR_ID(0xA1, 0x40, 0x17),
		.name = "FM25Q64A",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0xA1, 0x40, 0x18),
		.name = "FM25Q128A",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0xA1, 0x40, 0x19),
		.name = "FM25Q256I3",
		.size = SZ_32M,
		.flags = SPI_NOR_4B_OPCODES,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	},
};

const struct spi_nor_manufacturer spi_nor_fmsh = {
	.name = "fmsh",
	.parts = fmsh_parts,
	.nparts = ARRAY_SIZE(fmsh_parts),
};
