// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info normem_parts[] = {
	{
		.id = SNOR_ID(0x52, 0x21, 0x18),
		.name = "NM25Q128EVB",
		.size = SZ_16M,
		.no_sfdp_flags = SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ,
	},
};

static void normem_default_init(struct spi_nor *nor)
{
	nor->params->quad_enable = spi_nor_sr1_bit6_quad_enable;
}

static const struct spi_nor_fixups normem_fixups = {
	.default_init = normem_default_init,
};

const struct spi_nor_manufacturer spi_nor_normem = {
	.name = "normem",
	.parts = normem_parts,
	.nparts = ARRAY_SIZE(normem_parts),
	.fixups = &normem_fixups,
};
