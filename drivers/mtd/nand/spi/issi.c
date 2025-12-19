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

#define SPINAND_MFR_ISSI		0x9D

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 2, NULL, 0),
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

/*
 * ecc bits: 0xC0[4,6]
 * [0b000], No bit errors were detected;
 * [0b001] and [0b011], 1~6 Bit errors were detected and corrected. Not
 *	reach Flipping Bits;
 * [0b101], Bit error count equals the bit flip
 *	detection threshold
 * [0b010], Multiple bit errors were detected and
 *	not corrected.
 * others, Reserved.
 */
static int is37sml0xgb_ecc_get_status(struct spinand_device *spinand,
					u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	u8 eccsr = (status & GENMASK(6, 4)) >> 4;

	if (eccsr <= 1 || eccsr == 3)
		return eccsr;
	else if (eccsr == 5)
		return nanddev_get_ecc_requirements(nand)->strength;
	else
		return -EBADMSG;
}

static int is37smw04g_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = mtd->oobsize / 2;
	region->length = mtd->oobsize / 2;

	return 0;
}

static int is37smw04g_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = mtd->oobsize / 2;

	return 0;
}

static const struct mtd_ooblayout_ops is37smw04g_ooblayout = {
	.ecc = is37smw04g_ooblayout_ecc,
	.free = is37smw04g_ooblayout_free,
};

static const struct spinand_info issi_spinand_table[] = {
	SPINAND_INFO("IS37SML02G8B",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x24),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&is37smw04g_ooblayout, is37sml0xgb_ecc_get_status)),
};

static const struct spinand_manufacturer_ops issi_spinand_manuf_ops = {
};

const struct spinand_manufacturer issi_spinand_manufacturer = {
	.id = SPINAND_MFR_ISSI,
	.name = "ISSI",
	.chips = issi_spinand_table,
	.nchips = ARRAY_SIZE(issi_spinand_table),
	.ops = &issi_spinand_manuf_ops,
};
