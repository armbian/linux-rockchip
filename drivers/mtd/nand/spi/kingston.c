// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 *
 * Authors:
 *	Dingqiang Lin <jon.lin@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_KINGSTON		0x98

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int spi004_sdeg_ooblayout_ecc(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	if (section > 0)
		return -ERANGE;

	region->offset = mtd->oobsize / 2;
	region->length = mtd->oobsize / 2;

	return 0;
}

static int spi004_sdeg_ooblayout_free(struct mtd_info *mtd, int section,
				      struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* 2 bytes reserved for BBM */
	region->offset = 2;
	region->length = mtd->oobsize / 2 - 2;

	return 0;
}

static const struct mtd_ooblayout_ops spi004_sdeg_ooblayout = {
	.ecc = spi004_sdeg_ooblayout_ecc,
	.free = spi004_sdeg_ooblayout_free,
};

/*
 * ecc bits: 0xC0[4,6]
 * [0b000], No bit errors were detected;
 * [0b001, 0b101], 3~7 Bit errors were detected and corrected. Not
 *	reach Flipping Bits;
 * [0b110], Bit error count equals the bit flip detection threshold
 * [0b111], Bit errors greater than ECC capability(8 bits) and not corrected;
 */
static int spi004_sdeg_ecc_get_status(struct spinand_device *spinand, u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	u8 eccsr = (status & GENMASK(6, 4)) >> 4;

	if (eccsr == 0)
		return 0;
	else if (eccsr < 6)
		return eccsr + 2;
	else if (eccsr == 6)
		return nanddev_get_ecc_requirements(nand)->strength;
	else
		return -EBADMSG;
}

static const struct spinand_info kingston_spinand_table[] = {
	SPINAND_INFO("SPI001-SEEG",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x75),
		     NAND_MEMORG(1, 2048, 256, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&spi004_sdeg_ooblayout, spi004_sdeg_ecc_get_status)),
	SPINAND_INFO("SPI004-SDEG",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x53),
		     NAND_MEMORG(1, 4096, 256, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&spi004_sdeg_ooblayout, spi004_sdeg_ecc_get_status)),
};

static const struct spinand_manufacturer_ops kingston_spinand_manuf_ops = {
};

const struct spinand_manufacturer kingston_spinand_manufacturer = {
	.id = SPINAND_MFR_KINGSTON,
	.name = "kingston",
	.chips = kingston_spinand_table,
	.nchips = ARRAY_SIZE(kingston_spinand_table),
	.ops = &kingston_spinand_manuf_ops,
};
