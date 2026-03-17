// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Authors:
 *	Jon Lin <jon.lin@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_CHUCUN			0xD8

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int c5fxgm_ooblayout_ecc(struct mtd_info *mtd, int section,
				struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 64;
	region->length = 64;

	return 0;
}

static int c5fxgm_ooblayout_free(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 1;
	region->length = 63;

	return 0;
}

static const struct mtd_ooblayout_ops c5fxgm_ooblayout = {
	.ecc = c5fxgm_ooblayout_ecc,
	.free = c5fxgm_ooblayout_free,
};

static int c5fxgm_ecc_get_status(struct spinand_device *spinand, u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_HAS_BITFLIPS:
		return (nanddev_get_ecc_requirements(nand)->strength - 1);

	default:
		return nanddev_get_ecc_requirements(nand)->strength;
	}

	return -EINVAL;
}

static const struct spinand_info chucun_spinand_table[] = {
	SPINAND_INFO("C5F1GM7UE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x91),
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&c5fxgm_ooblayout, c5fxgm_ecc_get_status)),
	SPINAND_INFO("C5F1GM7RE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x81),
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&c5fxgm_ooblayout, c5fxgm_ecc_get_status)),
	SPINAND_INFO("C5F2GM7UE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x92),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&c5fxgm_ooblayout, c5fxgm_ecc_get_status)),
	SPINAND_INFO("C5F2GM7RE",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x82),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&c5fxgm_ooblayout, c5fxgm_ecc_get_status)),
};

static const struct spinand_manufacturer_ops chucun_spinand_manuf_ops = {
};

const struct spinand_manufacturer chucun_spinand_manufacturer = {
	.id = SPINAND_MFR_CHUCUN,
	.name = "CHUCUN",
	.chips = chucun_spinand_table,
	.nchips = ARRAY_SIZE(chucun_spinand_table),
	.ops = &chucun_spinand_manuf_ops,
};
