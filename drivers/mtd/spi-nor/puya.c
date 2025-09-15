// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info puya_parts[] = {
	{
		.id = SNOR_ID(0x85, 0x20, 0x18),
		.name = "PY25Q128HA",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x60, 0x17),
		.name = "P25Q64H",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x20, 0x17),
		.name = "PY25Q64HA",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x60, 0x18),
		.name = "P25Q128H",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x20, 0x19),
		.name = "PY25Q256HB",
		.size = SZ_32M,
		.flags = SPI_NOR_4B_OPCODES,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x65, 0x17),
		.name = "PY25Q64LB",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x65, 0x18),
		.name = "PY25Q128LA",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x85, 0x65, 0x19),
		.name = "PY25Q256LC",
		.size = SZ_32M,
		.flags = SPI_NOR_4B_OPCODES,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	},
};

const struct spi_nor_manufacturer spi_nor_puya = {
	.name = "puya",
	.parts = puya_parts,
	.nparts = ARRAY_SIZE(puya_parts),
};
