// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Authors:
 *	Dingqiang Lin <jon.lin@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_ZBIT		0x5E
#define ZBIT_STATUS_ECC_HAS_BITFLIPS_T	(3 << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 1, NULL, 0),
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

static int zb35q01b_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int zb35q01b_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = mtd->oobsize - 2;

	return 0;
}

static const struct mtd_ooblayout_ops zb35q01b_ooblayout = {
	.ecc = zb35q01b_ooblayout_ecc,
	.free = zb35q01b_ooblayout_free,
};

static const struct spinand_info zbit_spinand_table[] = {
	SPINAND_INFO("ZB35Q01BYIG",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0xA1),
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&zb35q01b_ooblayout, NULL)),
	SPINAND_INFO("ZB35Q04BYIG",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0xA3),
		     NAND_MEMORG(1, 2048, 128, 128, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&zb35q01b_ooblayout, NULL)),
};

static const struct spinand_manufacturer_ops zbit_spinand_manuf_ops = {
};

const struct spinand_manufacturer zbit_spinand_manufacturer = {
	.id = SPINAND_MFR_ZBIT,
	.name = "ZBIT",
	.chips = zbit_spinand_table,
	.nchips = ARRAY_SIZE(zbit_spinand_table),
	.ops = &zbit_spinand_manuf_ops,
};
