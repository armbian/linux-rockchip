// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info boya_parts[] = {
	{
		.id = SNOR_ID(0x68, 0x49, 0x19),
		.name = "BY25Q256FSEIG",
		.size = SZ_32M,
		.flags = SPI_NOR_4B_OPCODES,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x68, 0x40, 0x17),
		.name = "BY25Q64ESSIG",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	},
};

const struct spi_nor_manufacturer spi_nor_boya = {
	.name = "boya",
	.parts = boya_parts,
	.nparts = ARRAY_SIZE(boya_parts),
};
