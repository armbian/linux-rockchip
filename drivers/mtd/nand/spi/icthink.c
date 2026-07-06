// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Authors:
 *	Dingqiang Lin <jon.lin@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_ICTHINK		0x20

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

static int xs25d2gasl_ooblayout_ecc(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 48;
	region->length = 80;

	return 0;
}

static int xs25d2gasl_ooblayout_free(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 2;
	region->length = 46;

	return 0;
}

static const struct mtd_ooblayout_ops xs25d2gasl_ooblayout = {
	.ecc = xs25d2gasl_ooblayout_ecc,
	.free = xs25d2gasl_ooblayout_free,
};

static int xs25d2gasl_ecc_get_status(struct spinand_device *spinand,
				     u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_HAS_BITFLIPS:
		return 1;

	default:
		return nanddev_get_ecc_requirements(nand)->strength;
	}
}

static const struct spinand_info icthink_spinand_table[] = {
	SPINAND_INFO("XS25D2GASL",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x3),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
				      &write_cache_variants,
				      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xs25d2gasl_ooblayout, xs25d2gasl_ecc_get_status)),
};

static const struct spinand_manufacturer_ops icthink_spinand_manuf_ops = {
};

const struct spinand_manufacturer icthink_spinand_manufacturer = {
	.id = SPINAND_MFR_ICTHINK,
	.name = "ICTHINK",
	.chips = icthink_spinand_table,
	.nchips = ARRAY_SIZE(icthink_spinand_table),
	.ops = &icthink_spinand_manuf_ops,
};
