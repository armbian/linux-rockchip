// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info dosilicon_parts[] = {
	{
		.id = SNOR_ID(0xf8, 0x32, 0x17),
		.name = "FM25Q64A",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0xf8, 0x42, 0x18),
		.name = "FM25M4AA",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0xf8, 0x43, 0x17),
		.name = "FM25M64C",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}
};

const struct spi_nor_manufacturer spi_nor_dosilicon = {
	.name = "dosilicon",
	.parts = dosilicon_parts,
	.nparts = ARRAY_SIZE(dosilicon_parts),
};
