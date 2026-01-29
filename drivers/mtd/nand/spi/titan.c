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

#define SPINAND_MFR_TITAN		0x3D

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

static int tm1f0xguai_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = mtd->oobsize / 2;
	region->length = mtd->oobsize / 2;

	return 0;
}

static int tm1f0xguai_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = mtd->oobsize / 2 - 2;

	return 0;
}

static const struct mtd_ooblayout_ops tm1f0xguai_ooblayout = {
	.ecc = tm1f0xguai_ooblayout_ecc,
	.free = tm1f0xguai_ooblayout_free,
};

static int tm1f0xguai_ecc_get_status(struct spinand_device *spinand,
				     u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_HAS_BITFLIPS:
		return 0;
	default:
		return nanddev_get_ecc_requirements(nand)->strength;
	}

	return -EINVAL;
}

static const struct spinand_info titan_spinand_table[] = {
	SPINAND_INFO("TM1F02GBUAI",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x00, 0x32),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&tm1f0xguai_ooblayout, tm1f0xguai_ecc_get_status)),
};

static const struct spinand_manufacturer_ops titan_spinand_manuf_ops = {
};

const struct spinand_manufacturer titan_spinand_manufacturer = {
	.id = SPINAND_MFR_TITAN,
	.name = "TITAN",
	.chips = titan_spinand_table,
	.nchips = ARRAY_SIZE(titan_spinand_table),
	.ops = &titan_spinand_manuf_ops,
};

