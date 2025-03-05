// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2005, Intec Automation Inc.
 * Copyright (C) 2014, Freescale Semiconductor, Inc.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info esmt_nor_parts[] = {
	{
		.id = SNOR_ID(0x8c, 0x20, 0x16),
		.name = "f25l32pa",
		.size = SZ_4M,
		.flags = SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE,
		.no_sfdp_flags = SECT_4K,
	}, {
		.id = SNOR_ID(0x8c, 0x41, 0x16),
		.name = "f25l32qa-2s",
		.size = SZ_4M,
		.flags = SPI_NOR_HAS_LOCK,
		.no_sfdp_flags = SECT_4K,
	}, {
		.id = SNOR_ID(0x8c, 0x41, 0x17),
		.name = "f25l64qa",
		.size = SZ_8M,
		.flags = SPI_NOR_HAS_LOCK,
		.no_sfdp_flags = SECT_4K,
	}
	/* ESMT */
	{ "f25l32pa", INFO(0x8c2016, 0, 64 * 1024, 64)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "f25l32qa-2s", INFO(0x8c4116, 0, 64 * 1024, 64)
		FLAGS(SPI_NOR_HAS_LOCK)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "f25l64qa", INFO(0x8c4117, 0, 64 * 1024, 128)
		FLAGS(SPI_NOR_HAS_LOCK)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "en25qh128", INFO(0x1c7018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "en25s64", INFO(0x1c3817, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "en25qh256a", INFO(0x1c7019, 0, 64 * 1024, 512)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ)
		FIXUP_FLAGS(SPI_NOR_4B_OPCODES) },
	{ "en25qx256a", INFO(0x1c7119, 0, 64 * 1024, 512)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ)
		FIXUP_FLAGS(SPI_NOR_4B_OPCODES) },
	{ "en25qx128a", INFO(0x1c7118, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "en25qx64a", INFO(0x1c7117, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
};

const struct spi_nor_manufacturer spi_nor_esmt = {
	.name = "esmt",
	.parts = esmt_nor_parts,
	.nparts = ARRAY_SIZE(esmt_nor_parts),
};
