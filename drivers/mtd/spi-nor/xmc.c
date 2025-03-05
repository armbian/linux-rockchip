// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2005, Intec Automation Inc.
 * Copyright (C) 2014, Freescale Semiconductor, Inc.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info xmc_nor_parts[] = {
	{
		.id = SNOR_ID(0x20, 0x70, 0x17),
		.name = "XM25QH64A",
		.size = SZ_8M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	}, {
		.id = SNOR_ID(0x20, 0x70, 0x18),
		.name = "XM25QH128A",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	},
	/* XMC (Wuhan Xinxin Semiconductor Manufacturing Corp.) */
	{ "XM25QH64A", INFO(0x207017, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH128A", INFO(0x207018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH32A", INFO(0x207016, 0, 64 * 1024, 64)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH64C", INFO(0x204017, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QU64C", INFO(0x204117, 0, 64 * 1024, 128)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH128B", INFO(0x206018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH128C", INFO(0x204018, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QH256C", INFO(0x204019, 0, 64 * 1024, 512)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QU128C", INFO(0x204118, 0, 64 * 1024, 256)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "XM25QU256C", INFO(0x204119, 0, 64 * 1024, 512)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
};

/* XMC (Wuhan Xinxin Semiconductor Manufacturing Corp.) */
const struct spi_nor_manufacturer spi_nor_xmc = {
	.name = "xmc",
	.parts = xmc_nor_parts,
	.nparts = ARRAY_SIZE(xmc_nor_parts),
};
